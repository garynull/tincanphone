/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#include "Phone.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace tincan {


int Phone::mainLoop() throw()
{
	try
	{
		startup();

		while ( run() )
			continue;
	}
	catch (std::exception& ex)
	{
		Scopelock lock(mutex);
		stateOut = state = EXCEPTION;
		errorMessage = ex.what();
		if (updateHandler)
			updateHandler->sendUpdate();
		return 1;
	}
	catch (...)
	{
		Scopelock lock(mutex);
		stateOut = state = EXCEPTION;
		errorMessage = "Unknown exception";
		if (updateHandler)
			updateHandler->sendUpdate();
		return 1;
	}
	
	Scopelock lock(mutex);
	stateOut = state = EXITED;
	return 0;
}

Phone::Phone()
: commandIn(CMD_NONE),
  stateOut(STARTING),
  state(STARTING),
  address(),
  router(NULL),
  sock(-1),
  encoder(NULL),
  decoder(NULL),
  stream(NULL)
{
	// Init portaudio
	PaError paErr = Pa_Initialize();
	if (paErr)
		throw std::runtime_error(string("Could not start audio. Pa_Initialize error: ") + Pa_GetErrorText(paErr));
}

Phone::~Phone()
{
	// Close audio stream (ignore errors)
	if (stream)
		Pa_CloseStream(stream);

	// Cleanup opus
	if (decoder)
		opus_decoder_destroy(decoder);
	if (encoder)
		opus_encoder_destroy(encoder);

	// Close socket (ignore errors)
	if (sock != -1)
		Socket::close(sock);

	// Cleanup UPnP
	delete router;
	
	// Cleanup portaudio (ignore errors)
	Pa_Terminate();
}

void Phone::startup()
{
	{
		Scopelock lock(mutex);
		logOut += "Starting up, please wait...\n";
		if (updateHandler)
			updateHandler->sendUpdate();
	}


	// Generate sound buffers
	
	// The sound of silence - Simon and Garfunkel not required
	memset(silence, 0, sizeof(silence));

	// Note that the tone frequencies should fit evenly into a single 20ms sample (ie. be multiples of 50)
	static const float pi2 = 2.f * 3.14159265f;
	static const float amp16 = 0.5f * float(std::numeric_limits<opus_int16>::max());

	// 400hz
	for (uint s = 0; s < PACKET_SAMPLES; ++s) {
		float x = float(s) / float(SAMPLE_RATE);
		ringToneIn[s] = opus_int16( sin(x * 400.f * pi2) * amp16 );
	}

	// 250hz
	for (uint s = 0; s < PACKET_SAMPLES; ++s) {
		float x = float(s) / float(SAMPLE_RATE);
		ringToneOut[s] = opus_int16( sin(x * 250.f * pi2) * amp16 );
	}


	// Setup socket
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1)
		throw std::runtime_error("Failed to create socket");

	Socket::setBlocking(sock, false);


	// Bind local port
	uint16 localPort = PORT_DEFAULT;
	for (;;)
	{
		sockaddr_in bindaddr;
		bindaddr.sin_family = AF_INET;
		bindaddr.sin_addr.s_addr = INADDR_ANY;
		bindaddr.sin_port = htons(localPort);
		if ( bind(sock, (sockaddr*)&bindaddr, sizeof(bindaddr)) )
		{
			if (Socket::getError() != EADDRINUSE)
				throw std::runtime_error("Could not bind UDP port " + toString(localPort) + ": " + Socket::getErrorString());
			++localPort;
			if (localPort > PORT_MAX)
				throw std::runtime_error("Could not find an available local port");
		}
		else
		{
			break;
		}
	}


	// Open WAN port via Router
	uint16 wanPort = PORT_DEFAULT;
	try
	{
		router = new Router(UPNP_TIMEOUT_MS);

		for (;;)
		{
			// Returns FALSE if port in use
			if ( !router->setPortMapping(localPort, wanPort, Router::MAP_UDP, "Tin Can Phone") )
			{
				wanPort++;
				if (wanPort > PORT_MAX)
					throw std::runtime_error("Could not find an available port on router");
			}
			else
			{
				break;
			}
		}
	}
	catch (std::runtime_error& ex)
	{
		// Log error but complete startup
		log << "*** ERROR: " << ex.what() << ". You may need to forward UDP port " << localPort << " manually." << endl;
		state = HUNGUP;
		return;
	}


	// Only specify port in log if it's not the default
	string portstr;
	if (wanPort != PORT_DEFAULT)
		portstr = string(":") + toString(wanPort);
	
	// Done starting up
	state = HUNGUP;
	log << "Ready! Your IP address is: " << router->getWanAddress() << portstr << endl;
}

bool Phone::run()
{
	Command command;

	// Synchronize input and output
	{
		Scopelock lock(mutex);
		
		// Set logOut and clear log
		logOut += log.str();
		log.str(string());

		// Set stateOut, and notify updateHandler
		if (stateOut != state)
		{
			stateOut = state;
			if (updateHandler)
				updateHandler->sendUpdate();
		}
		
		// Get commandIn and clear
		command = commandIn;
		commandIn = CMD_NONE;
		
		// Get addressIn when CMD_CALL
		if (command == CMD_CALL)
		{
			addrinfo hints = {};
			hints.ai_flags = AI_NUMERICHOST; //"suppresses any potentially lengthy network host address lookups"
			hints.ai_family = AF_UNSPEC;     //Allow AF_INET or AF_INET6
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;

			addrinfo* result = NULL;

			string port = toString(PORT_DEFAULT);
			size_t colon = addressIn.find(':');
			if (colon != string::npos) {
				port = addressIn.substr(colon+1);
				addressIn.resize(colon);
				log << addressIn << ' ' << port << endl;
			}

			int error = getaddrinfo(addressIn.c_str(), port.c_str(), &hints, &result);
			if (!error)
			{
				// Set 'address'
				assert(result->ai_addrlen <= sizeof address);
				memcpy(&address, result->ai_addr, result->ai_addrlen);
			}
			else
			{
				log << "Invalid IP address" << endl;
				command = CMD_NONE; //Cancel command since input invalid
			}

			freeaddrinfo(result);
		}
	}

	// Handle commands
	if (command == CMD_CALL && (state == HUNGUP || state == RINGING))
	{
		dial();
	}
	else if (command == CMD_ANSWER && state == RINGING)
	{
		goLive();
	}
	else if (command == CMD_HANGUP && (state == DIALING || state == LIVE))
	{
		hangup();
	}
	else if (command == CMD_EXIT)
	{
		if (state == LIVE)
			hangup();
		
		return false;
	}


	// Handle incoming packets
	Packet packet = {};
	
	// Loop until EWOULDBLOCK
	for (;;)
	{
		sockaddr_storage fromAddr = {};
		socklen_t fromAddrLen = sizeof(fromAddr);
		int received = recvfrom(sock, (char*)&packet, sizeof(packet), 0, (sockaddr*)&fromAddr, &fromAddrLen);
		if (received >= int(sizeof(packet.header)))
		{
			packet.header = ntohl(packet.header);
			packet.seq =    ntohl(packet.seq);
			receivePacket(packet, received, fromAddr);
		}
		else if (received < 0)
		{
			if (Socket::getError() == EWOULDBLOCK)
				break;
		
			if (Socket::getError() == ECONNABORTED || Socket::getError() == ECONNRESET)
			{
				log << "Network error: " << Socket::getErrorString() << endl;
				hangup();
			}
			else
			{
				throw std::runtime_error("recvfrom error: " + Socket::getErrorString());
			}
		}
	}


	if (state == DIALING)
	{
		// Send RING packet repeatedly
		if (ringPacketTimer >= RING_PACKET_INTERVAL)
		{
			ringPacketTimer = 0;
			sendPacket(Packet::RING, address);
		}

		// Audio playblack is blocking
		playRingtone();

		ringPacketTimer += PACKET_MS;
	}
	else if (state == RINGING)
	{
		// Stop ringing if we're no longer getting packets
		if (ringPacketTimer > RING_PACKET_INTERVAL*2)
		{
			log << "Missed call from " << address << endl;
			endAudioStream();
			state = HUNGUP;
		}
		else
		{
			// Audio playblack is blocking
			playRingtone();

			ringPacketTimer += PACKET_MS;
		}
	}
	else if (state == LIVE)
	{
		// Read microphone stream and send packets
		while (Pa_GetStreamReadAvailable(stream) >= PACKET_SAMPLES)
		{
			// The 'frames' param of Pa_ReadStream should match 'framesPerBuffer' param of Pa_OpenStream
			opus_int16 microphone[PACKET_SAMPLES];
			PaError paErr = Pa_ReadStream(stream, microphone, PACKET_SAMPLES);
			if (paErr && paErr != paInputOverflowed)
				throw std::runtime_error(string("Pa_ReadStream error: ") + Pa_GetErrorText(paErr));

			// Compress and send
			Packet sendbuf;
			sendbuf.header = htonl(Packet::AUDIO);
			sendbuf.seq =    htonl(sendseq);
			
			++sendseq;

			opus_int32 enc = opus_encode(encoder, microphone, PACKET_SAMPLES, sendbuf.data, sizeof(sendbuf.data));
			if (enc < 0)
				throw std::runtime_error(string("opus_encode error: ") + opus_strerror(enc));
			
			int sendsize = sizeof(packet.header) + sizeof(packet.seq) + enc;
			sendPacket((char*)&sendbuf, sendsize, address);
		}

		// Play any downloaded and buffered audio
		playReceivedAudio();
	}
	else
	{
		// If no blocking audio calls to do, sleep instead
		Pa_Sleep(PACKET_MS);
	
		return true;
	}
	
	return true;
}

void Phone::hangup()
{
	assert(state != HUNGUP);

	log << "Hanging up" << endl;

	if (stream)
		endAudioStream();

	if (state == LIVE)
	{
		opus_decoder_destroy(decoder);
		decoder = NULL;
		opus_encoder_destroy(encoder);
		encoder = NULL;

		audiobuf.clear();
	}

	state = HUNGUP;
}

void Phone::dial()
{
	assert(state != DIALING);
	log << "Dialing " << address << endl;
	ringToneTimer = 0;
	ringPacketTimer = 0;
	state = DIALING;
	beginAudioStream(false, true);
}

void Phone::startRinging()
{
	assert(state != RINGING);
	log << "*** Incoming call from " << address << endl;
	ringToneTimer = 0;
	ringPacketTimer = 0;
	state = RINGING;
	beginAudioStream(false, true);
}

void Phone::goLive()
{
	assert(state != LIVE);

	sendseq = 1;
	audiobuf.resize(1);
	audiobuf.front().seq = 1;
	disconnectTimer = 0;
	increaseBuffering = true;
	missedPackets = 0;

	// Initialize opus
	int opusErr;
	encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &opusErr);
	if (opusErr != OPUS_OK)
		throw std::runtime_error(string("opus_encoder_create error: ") + opus_strerror(opusErr));

	decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &opusErr);
	if (opusErr != OPUS_OK)
		throw std::runtime_error(string("opus_decoder_create error: ") + opus_strerror(opusErr));

	// Start portaudio stream
	log << "*** Call started" << endl;
	log << "Sound in: " << Pa_GetDeviceInfo( Pa_GetDefaultInputDevice() )->name << endl;
	log << "Sound out: " << Pa_GetDeviceInfo( Pa_GetDefaultOutputDevice() )->name << endl;
	beginAudioStream(true, true);

	// Now LIVE
	state = LIVE;
}

void Phone::receivePacket(const Packet& packet, uint packetSize, const sockaddr_storage& fromAddr)
{
	switch (packet.header)
	{
	case Packet::RING:
		if (state == HUNGUP)
		{
			// Incoming call!
			address = fromAddr;
			startRinging();
		}
		else if (fromAddr == address)
		{
			if (state == RINGING)
				ringPacketTimer = 0; //Reset timer
			else if (state == DIALING)
				goLive(); //We're both dialing each other at the same time?
		}
		else
		{
			// We can't accept new incoming calls right now
			sendPacket(Packet::BUSY, fromAddr);
		}
		break;
		
	case Packet::BUSY:
		if (state == DIALING && fromAddr == address)
		{
			log << "*** " << address << " is busy" << endl;
			hangup();
		}
		break;
		
	case Packet::AUDIO:
		if (fromAddr != address || state == HUNGUP)
		{
			// Not in a call with sender, tell them we've hung up
			sendPacket(Packet::HANGUP, fromAddr);
		}
		else if (state == DIALING)
		{
			goLive();
			bufferReceivedAudio(packet, packetSize);
		}
		else if (state == LIVE)
		{
			bufferReceivedAudio(packet, packetSize);
		}
		break;
		
	case Packet::HANGUP:
		if (state != HUNGUP && fromAddr == address)
		{
			log << "*** " << address << " has hung up" << endl;
			hangup();
		}
		break;
		
	default:
		//Ignore packet
		break;
	}
}

void Phone::bufferReceivedAudio(const Packet& packet, uint packetSize)
{
	// Discard packet if too small
	if (packetSize <= offsetof(Packet,data))
		return;

	// Discard late packets
	if (packet.seq < audiobuf.front().seq)
		return;

	// Make sure audio buffer is expanded to packet.seq
	while (audiobuf.back().seq < packet.seq)
	{
		uint32 prevseq = audiobuf.back().seq;
		audiobuf.resize(audiobuf.size() + 1);
		audiobuf.back().seq = prevseq + 1;
	}

	// Find position in buffer for packet.seq and memcpy the packet into place
	AudioBuffer::iterator p = std::find(audiobuf.begin(), audiobuf.end(), packet.seq);
	p->datasize = packetSize - offsetof(Packet,data);
	memcpy(p->data, packet.data, p->datasize);
}

void Phone::playReceivedAudio()
{
	if (increaseBuffering && audiobuf.size() < BUFFERED_PACKETS_MAX)
	{
		if (audiobuf.size() == 1 && !audiobuf.front().datasize)
		{
			disconnectTimer += PACKET_MS;
			if (disconnectTimer > DISCONNNECT_TIMEOUT)
			{
				log << "*** Call disconnected!" << endl;
				hangup();
				return;
			}
		}
		else
		{
			log << "Buffering increased" << endl;
			increaseBuffering = false;
		}
		
		writeAudioStream(silence, PACKET_SAMPLES);
		return;
	}

	opus_int16 decoded[PACKET_SAMPLES];
	opus_int32 decodeRet;

	if (audiobuf.front().datasize)
	{
		// Decode a packet from the front of the buffer
		AudioPacket& front = audiobuf.front();
		decodeRet = opus_decode(decoder, front.data, front.datasize, decoded, PACKET_SAMPLES, 0);
		if (decodeRet == OPUS_INVALID_PACKET)
		{
			log << "Corrupt packet " << front.seq << endl;
			// Try again by treating the packet as lost
			decodeRet = opus_decode(decoder, NULL, 0, decoded, PACKET_SAMPLES, 0);
		}
		else
		{
			// Successfully played an audio packet
			missedPackets = 0;
			disconnectTimer = 0;
		}
	}
	else
	{
		// No data for packet at this seq
		
		log << "Missing packet " << audiobuf.front().seq << endl;

		++missedPackets;

		// Start buffering if we're below the minimum, or there are 2 consecutive missed packets
		if (audiobuf.size() < BUFFERED_PACKETS_MIN || (missedPackets > 1 && audiobuf.size() < BUFFERED_PACKETS_MAX))
			increaseBuffering = true;

		decodeRet = opus_decode(decoder, NULL, 0, decoded, PACKET_SAMPLES, 0);
	}

	// Check for Opus error from above
	if (decodeRet < 0)
		throw std::runtime_error(string("opus_decode failed: ") + opus_strerror(decodeRet));

	// Pop the packet we just decoded
	if (audiobuf.size() == 1)
	{
		// Leave at least one space in buffer at correct seq
		audiobuf.front().datasize = 0;
		audiobuf.front().seq++;
	}
	else
	{
		audiobuf.pop_front();
	}

	// If too many packets are buffered, "skip ahead" to reduce latency
	// Note that the packet has been decoded (as opus requires), but we don't play it
	if (audiobuf.size() >= BUFFERED_PACKETS_MAX)
	{
		log << "Reducing buffering" << endl;
		playReceivedAudio(); //Play the next packet immediately
		return;
	}

	// Play the decoded packet
	writeAudioStream(decoded, PACKET_SAMPLES);
}

void Phone::playRingtone()
{
	enum {RING_MS = 400, RING_PAUSE = 800, RING_REPEAT = 3800};
	const uint toneTime = ringToneTimer % RING_REPEAT;

	// Play tone twice for RING_MS, with a RING_PAUSE
	if ((toneTime >= 0 && toneTime < RING_MS) || (toneTime >= RING_PAUSE && toneTime < RING_PAUSE+RING_MS))
		writeAudioStream((state == RINGING) ? ringToneIn : ringToneOut, PACKET_SAMPLES);
	else
		writeAudioStream(silence, PACKET_SAMPLES);
	
	ringToneTimer += PACKET_MS;
}

void Phone::sendPacket(char* buffer, int size, const sockaddr_storage& to)
{
	const int tosize = (to.ss_family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
	int sent = sendto(sock, buffer, size, 0, (sockaddr*)&to, tosize);
	if (sent < 0)
	{
		log << "sendto error: " << Socket::getErrorString() << endl;
		int error = Socket::getError();
		if (error != EWOULDBLOCK && error != ECONNABORTED && error != ECONNRESET)
			throw std::runtime_error("sendto error: " + Socket::getErrorString());
	}
}

void Phone::beginAudioStream(bool input, bool output)
{
	assert(input || output);

	if (stream)
		endAudioStream();

	const int inChannels = input ? CHANNELS : 0;
	const int outChannels = output ? CHANNELS : 0;

	PaError paErr;
	paErr = Pa_OpenDefaultStream(&stream, inChannels, outChannels, paInt16, SAMPLE_RATE, PACKET_SAMPLES, NULL, NULL);
	if (paErr)
		throw std::runtime_error(string("Pa_OpenDefaultStream error: ") + Pa_GetErrorText(paErr));

	paErr = Pa_StartStream(stream);
	if (paErr)
		throw std::runtime_error(string("Pa_StartStream error: ") + Pa_GetErrorText(paErr));
}

void Phone::writeAudioStream(void* buffer, ulong samples)
{
	assert(stream);
	PaError paErr = Pa_WriteStream(stream, buffer, samples);
	if (paErr != paNoError)
	{
		if (paErr == paOutputUnderflowed)
			log << "Pa_WriteStream output underflowed" << endl;
		else
			throw std::runtime_error(string("Pa_WriteStream failed: ") + Pa_GetErrorText(paErr));
	}
}

void Phone::endAudioStream()
{
	assert(stream);
	PaError paErr = Pa_CloseStream(stream);
	if (paErr)
		throw std::runtime_error(string("Pa_CloseStream error: ") + Pa_GetErrorText(paErr));
	stream = NULL;
}


}
