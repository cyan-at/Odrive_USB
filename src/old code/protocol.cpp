#include "protocol.h"


void send_to_odrive(libusb_device_handle* handle, std::vector<uint8_t>& packet, int* sent_bytes) {
	libusb_bulk_transfer(handle,
		(1 | LIBUSB_ENDPOINT_OUT),
		packet.data(),
		packet.size(),
		sent_bytes,
		0);
}

void receive_from_odrive(libusb_device_handle* handle, unsigned char* packet, int max_bytes_to_receive, int* received_bytes, int timeout = 0) {
	libusb_bulk_transfer(handle,
		(1 | LIBUSB_ENDPOINT_IN),
		packet,
		max_bytes_to_receive,
		received_bytes,
		timeout);
}


// Endpoint request with std::vector<uint8_t> payload
int odrive_endpoint_request(libusb_device_handle* handle, int endpoint_id, serial_buffer& received_payload, std::vector<uint8_t> payload, int ack, int length) {
	serial_buffer send_buffer;
	serial_buffer receive_buffer;
	unsigned char receive_bytes[100] = { 0 };
	int sent_bytes = 0;
	int received_bytes = 0;
	int max_bytes_to_receive = 64;
	int timeout = 1000;
	int i = 0;
	static short seq_no = 0;
	short received_seq_no = 0;
	int CRC = 7230;

	if (ack) {
		endpoint_id |= 0x8000;
	}
	seq_no = (seq_no + 1) & 0x7fff;
	seq_no |= 0x80;

	// Send the packet
	send_to_odrive(handle, create_odrive_packet(seq_no, endpoint_id, (short)length, (std::vector<uint8_t>)payload), &sent_bytes);


	// Immediatly wait for response from Odrive and check if ack (if we asked for one)
	receive_from_odrive(handle, receive_bytes, max_bytes_to_receive, &received_bytes, timeout);

	for (i = 0; i < received_bytes; i++) {
		receive_buffer.push_back(receive_bytes[i]);
	}

	serial_buffer::iterator it = receive_buffer.begin();
	received_payload = decode_odrive_packet(it, received_seq_no, receive_buffer);

	// return the response payload
	return received_payload.size();
}


void get_odrive_json_interface(libusb_device_handle* handle, json& j) {
	serial_buffer send_payload;
	serial_buffer receive_payload;
	serial_buffer received_json;

	int received_bytes = 0;
	int total_received = 0;

	do {
		serialize(send_payload, (int)total_received);
		received_bytes = odrive_endpoint_request(handle, 0, receive_payload, send_payload, 1, 64);
		send_payload.clear();
		total_received += received_bytes;
		for (uint8_t byte : receive_payload) {
			received_json.push_back(byte);
		}
	} while (received_bytes > 0);
	j = json::parse(received_json);
	printf("Received %i bytes!\n", total_received);
}


void odrive_set_value_float(libusb_device_handle* handle, int id, float value) {
	serial_buffer received_payload;
	serial_buffer send_payload;

	serialize(send_payload, (float)value);
	odrive_endpoint_request(handle, id, received_payload, send_payload, 1, 4);
}


void odrive_get_value_float(libusb_device_handle* handle, int id, float& value) {
	serial_buffer received_payload;
	serial_buffer send_payload;
	serialize(send_payload, (int)0);

	odrive_endpoint_request(handle, id, received_payload, send_payload, 1, 4);
	deserialize(received_payload.begin(), value);
}



void parse_endpoints2(json& j, Endpoint& endpoints) {
	for (json& obj : j) {
		std::string name = obj["name"];
		std::string type = obj["type"];
		int id = obj["id"];
		//printf("id: %i\tname: %s\ttype: %s\n", id, name.c_str(), type.c_str());

		endpoints.add_child(name, type, id);

		if (obj.count("members")) {
			json& members = obj["members"];
			parse_endpoints2(members, endpoints[name]);
		}
	}
}