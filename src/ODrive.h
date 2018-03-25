#pragma once
#include <string>
#include <vector>
#include <map>
#include "libusb-1.0\libusb.h"
#include <json.hpp>
#include "endpoint.h"


#define VID     0x1209
#define PID     0x0D32

typedef std::vector<uint8_t> serial_buffer;
using nlohmann::json;



class Protocol {
public:

	Protocol() {
		libusb_init(&ctx);
		handle = libusb_open_device_with_vid_pid(ctx, VID, PID);
		int r = libusb_claim_interface(handle, 1);
	}
	~Protocol() {
		libusb_release_interface(handle, 1);
		libusb_close(handle);
		libusb_exit(ctx);
	}

	int endpoint_request(int endpoint_id, serial_buffer& received_payload, std::vector<uint8_t> payload, int ack, int length);
	Endpoint get_json_interface();
	void get_float(int id, float& value);
	void get_int(int id, int& value);
	void set_float(int id, float& value);
	void set_int(int id, int& value);


private:
	libusb_device_handle * handle;
	libusb_context* ctx;
	json j;

	struct odrive_packet {
		short sequence_number;
		short endpoint_id;
		short payload_length;
		std::vector<uint8_t> payload;
		short CRC16;
	};

	void send_to_odrive(libusb_device_handle* handle, std::vector<uint8_t>& packet, int* sent_bytes);

	void receive_from_odrive(libusb_device_handle* handle, unsigned char* packet, int max_bytes_to_receive, int* received_bytes, int timeout = 0);

	// Templates for basic serialization and deserialization
	template <class T>
	void serialize(serial_buffer&, const T&);

	template <class T>
	void deserialize(serial_buffer::iterator&, T&);


	void serialize(serial_buffer& serial_buffer, const int& value);

	void serialize(serial_buffer& serial_buffer, const float& valuef);

	void serialize(serial_buffer& serial_buffer, const short& value);

	void serialize(serial_buffer& serial_buffer, const std::vector<uint8_t>& value);

	void deserialize(serial_buffer::iterator& it, short& value);

	void deserialize(serial_buffer::iterator& it, int& value);

	void deserialize(serial_buffer::iterator& it, float& value);

	void deserialize(serial_buffer::iterator& it, std::vector<uint8_t>& value);



	// Variadic template to allow multiple arguments
	template <class T, class U, class ...Us>
	void serialize(serial_buffer& serial_buffer, const T& t, const U& u, const Us&... us) {
		serialize(serial_buffer, t);
		serialize(serial_buffer, u, us...);
	}


	template <class T, class U, class ...Us>
	void deserialize(serial_buffer::iterator& it, T& t, U& u, Us&... us) {
		deserialize(it, t);
		deserialize(it, u, us...);
	}


	// Struct serialization
	void serialize(serial_buffer& serial_buffer, const odrive_packet& odrive_packet);
	void deserialize(serial_buffer::iterator& it, odrive_packet& odrive_packet);

	template <class ...Ts>
	serial_buffer create_odrive_packet(short seq_no, int endpoint, short response_size, const Ts&... ts) {
		serial_buffer payload;
		serialize(payload, ts...);

		serial_buffer data;
		short crc = 0;
		if ((endpoint & 0x7fff) == 0) {
			crc = 1;
		}
		else {
			crc = 7230;
		}
		serialize(data, (short)seq_no, (short)endpoint, (short)response_size);

		for (uint8_t b : payload) {
			data.push_back(b);
		}

		serialize(data, (short)crc);

		return data;
	}

	serial_buffer decode_odrive_packet(serial_buffer::iterator& it, short& seq_no, serial_buffer& received_packet);


};