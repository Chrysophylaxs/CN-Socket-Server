#include <winsock2.h>
#include <thread>
#include <iostream>

// Stop variable
bool stop = false;

// Stop thread
void read() {
	std::string input;
	while (!stop) {
		if (input != "STOP") {
			std::cin >> input;
		}
		else {
			stop = true;
			std::cout << "===================================" << std::endl;
			std::cout << "Shutting down...";
		}
	}
}

// Server
int main() {
	// General Variables
	WSADATA wsaData;
	std::thread readThread(read);

	// Client info
	const int maxClients = 10;
	std::string clientName[2 * maxClients];
	SOCKET clientSocket[maxClients]; // TCP
	sockaddr_in clientAddr[maxClients]; // UDP

	// Server variables
	SOCKET serverSocketTCP, serverSocketUDP, incomingSocket, temp;
	sockaddr_in server{}, client{}, recipient{};
	int addrLength = sizeof(sockaddr_in);

	// Variables for select
	fd_set readfds;
	timeval tv{};
	tv.tv_sec = 0;
	tv.tv_usec = 5000;

	// Variables for sending and receiving
	int bytes, bytesSent;
	const int bufSize = 2048;
	auto *buffer = new char[bufSize + 1];
	
	// Response codes
	std::string response;
	const char* SEND_OK = "SEND-OK\n";
	const char* UNKNOWN = "UNKNOWN\n";
	const char* IN_USE = "IN-USE\n";
	const char* BUSY = "BUSY\n";
	const char* BAD_RQST_HDR = "BAD-RQST-HDR\n";
	const char* BAD_RQST_BODY = "BAD-RQST-BODY\n";
	const char* SERVER_ERROR = "SERVER-ERROR\n";

	// Initializing clients to zero
	for (int i = 0; i < maxClients; i++) {
		clientSocket[i] = 0; // TCP
		clientAddr[i].sin_addr.s_addr = inet_addr("0.0.0.0"); // UDP
	}

	// Initializing WSA
	std::cout << "Initialising WSA...";
	int wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaError != 0) {
		std::cout << " > Failed Initialising. Code: " << wsaError << std::endl;
		return 1;
	}
	std::cout << " > Initialised." << std::endl;

	// Creating server TCP and UDP socket
	std::cout << "Creating sockets...";
	serverSocketTCP = socket(AF_INET, SOCK_STREAM, 0); // TCP
	serverSocketUDP = socket(AF_INET, SOCK_DGRAM, 0); // UDP
	if (serverSocketTCP == INVALID_SOCKET || serverSocketUDP == INVALID_SOCKET) {
		std::cout << " > Failed Creating. Code: " << WSAGetLastError() << std::endl;
		return 1;
	}
	std::cout << " > Sockets created." << std::endl;

	// Setting server address and port
	server.sin_family = AF_INET;
	// server.sin_addr.s_addr = inet_addr("145.108.76.59");
    // server.sin_addr.s_addr = inet_addr("192.168.178.28");
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(8888);

	// Binding sockets to server address and port
	std::cout << "Binding sockets...";
	if (bind(serverSocketTCP, (sockaddr*)&server, addrLength) == SOCKET_ERROR) {
		std::cout << " > Failed Binding. Code: " << WSAGetLastError() << std::endl;
		return 1;
	}
	if (bind(serverSocketUDP, (sockaddr*)&server, addrLength) == SOCKET_ERROR) {
		std::cout << " > Failed Binding. Code: " << WSAGetLastError() << std::endl;
		return 1;
	}
	std::cout << " > Sockets bound." << std::endl;

	// Setting TCP to listen for connections
	listen(serverSocketTCP, SOMAXCONN);
	std::cout << "Listening for incoming connections." << std::endl;
	std::cout << "===================================" << std::endl;

	// Loop until stop
	while (!stop) {
		// Set Server and Client sockets to be selected
		FD_ZERO(&readfds);
		FD_SET(serverSocketTCP, &readfds);
		FD_SET(serverSocketUDP, &readfds);

		for (int i = 0; i < maxClients; i++) {
			if (clientSocket[i] > 0) {
				FD_SET(clientSocket[i], &readfds);
			}
		}

		// Select
		if (select(0, &readfds, nullptr, nullptr, &tv) == SOCKET_ERROR) {
			std::cout << "Failed Selecting. Code: " << WSAGetLastError() << std::endl;
			stop = true;
		}
		// TCP Socket is set: Incoming connection
		else if (FD_ISSET(serverSocketTCP, &readfds)) {
			// Accept connection
			incomingSocket = accept(serverSocketTCP, (sockaddr*)&client, &addrLength);
			std::cout << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port);
			if (incomingSocket == INVALID_SOCKET) {
				std::cout << " -> Failed Accepting connection. Code: " << WSAGetLastError() << std::endl;
				continue;
			}

			// Check if there are already maxClients
			int num = 0;
			while (num < maxClients && clientSocket[num] > 0) {
				num++;
			}
			if (num == maxClients) {
				// Server is busy
				bytesSent = send(incomingSocket, BUSY, (int)strlen(BUSY), 0);
				if (bytesSent == SOCKET_ERROR) {
					std::cout << " <- Error sending message, code:" << WSAGetLastError() << std::endl;
				}
				else {
					std::cout << " <- BUSY" << std::endl;
				}
				closesocket(incomingSocket);
			}
			else {
				clientSocket[num] = incomingSocket;
				std::cout << " -> Connected." << std::endl;
			}
		}
		// UDP Socket is set: Incoming datagram
		else if (FD_ISSET(serverSocketUDP, &readfds)) {
			// Receive datagram
			bytes = recvfrom(serverSocketUDP, buffer, bufSize, 0, (struct sockaddr*)&client, &addrLength);
			std::cout << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port);

			// Something went wrong
			if (bytes == SOCKET_ERROR || bytes == 0) {
				// Check if sender was logged, in which case they disconnected
				int num = 0;
				while (num < maxClients && clientAddr[num].sin_addr.s_addr != client.sin_addr.s_addr) {
					num++;
				}
				if (num == maxClients) {
					std::cout << " <- Error receiving message, code:" << WSAGetLastError() << std::endl;
				}
				else {
					std::cout << " -> Disconnected." << std::endl;
					clientAddr[num].sin_addr.s_addr = inet_addr("0.0.0.0");
					clientName[maxClients + num] = "";
				}
			}
			// Process the message
			else {
				// Convert buffer to string
				buffer[bytes] = '\0';
				std::string msg = buffer;
				std::cout << " -> " << msg;
				// Handshake
				if (msg.size() > 10 && msg.substr(0, 10) == "HELLO-FROM") {
					if (msg.size() > 11) {
						// Check if sender was logged
						bool logged;
						int num = 0;
						while (num < maxClients && clientAddr[num].sin_addr.s_addr != client.sin_addr.s_addr) {
							num++;
						}
						if (num == maxClients) {
							logged = false;
						}
						else {
							logged = true;
						}
						// Extract name from message
						std::string name;
						int index = 11;
						while (index < msg.size() && !isspace(msg.at(index))) {
							name += msg.at(index);
							index++;
						}
						
						// Check if name already in use
						bool inuse = false;
						if (name == "echobot") {
							inuse = true;
						}
						else {
							for (int i = 0; i < maxClients; i++) {
								if (name == clientName[i]) {
									inuse = true;
									break;
								}
							}
						}
						if (inuse) {
							response = IN_USE;
						}
						else {
							// If logged, change name
							if (logged) {
								// response = "HELLO " + name + "\n";
								clientName[maxClients + num] = name;
							}
							else {
								// Check for a non-used spot in the clients
								int i = 0;
								while (i < maxClients && clientAddr[i].sin_addr.s_addr != inet_addr("0.0.0.0")) {
									i++;
								}
								// There are already max clients
								if (i == maxClients) {
									response = BUSY;
								}
								// Store their info
								else {
									response = "HELLO " + name + "\n";
									clientAddr[i] = client;
									clientName[maxClients + i] = name;
								}
							}
						}
					}
					// No name supplied
					else {
						response = BAD_RQST_BODY;
					}
				}
				// Online users
				else if (msg == "WHO\n") {
					// Check if the client logged in yet
					bool logged;
					int num = 0;
					while (num < maxClients && clientAddr[num].sin_addr.s_addr != client.sin_addr.s_addr) {
						num++;
					}
					// std::cout << num << '\n';
					if (num == maxClients) {
						logged = false;
					}
					else {
						logged = true;
					}
					// Only if they have logged in
					if (logged) {
						response = "WHO-OK echobot";
						// Loop through all names 
						for (int i = 0; i < 2 * maxClients; i++) {
							if (!clientName[i].empty()) {
								response += ",";
								response += clientName[i];
							}
						}
						response += '\n';
					}
					else {
						response = BAD_RQST_HDR;
					}
				}
				// Send a message
				else if (msg.size() > 4 && msg.substr(0, 4) == "SEND") {
					// Check if the client logged in yet
					bool logged;
					int num = 0;
					while (num < maxClients && clientAddr[num].sin_addr.s_addr != client.sin_addr.s_addr) {
						num++;
					}
					if (num == maxClients) {
						logged = false;
					}
					else {
						logged = true;
					}
					// Only if they have logged in
					if (logged) {
						// Extract user and message from msg
						std::string user, message;
						int i = 5;
						while (msg.at(i) != ' ') {
							user += msg.at(i);
							i++;
						}
						while (i < msg.size() - 1) {
							i++;
							message += msg.at(i);
						}
						// Check if they are trying to send a message to themselves
						bool self = false;
						for (int i = maxClients; i < 2 * maxClients; i++) {
							if (clientName[i] == user && inet_ntoa(clientAddr[i].sin_addr) == inet_ntoa(client.sin_addr)) {
								self = true;
							}
						}
						// Check if user or message is invalid
						if (user.empty() || message.empty()) {
							response = BAD_RQST_BODY;
						}
						// They send a message to echobot or themselves
						else if (user == "echobot" || self) {
							bytesSent = sendto(serverSocketUDP, SEND_OK, (int)strlen(SEND_OK), 0, (sockaddr*)&client, addrLength);
							std::cout << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port);
							// Logging
							if (bytesSent == SOCKET_ERROR) {
								std::cout << " <- Error sending message, code:" << WSAGetLastError() << std::endl;
							}
							else {
								std::cout << " <- SEND-OK" << std::endl;
							}
							response = "DELIVERY " + user + " " + message;
						}
						// Message to another client
						else {
							// Check if the recipient exists
							bool inuse = false;
							int j;
							for (j = 0; j < 2 * maxClients; j++) {
								if (clientName[j] == user) {
									inuse = true;
									break;
								}
							}
							// Deliver the message
							if (inuse) {
								std::string delivery = "DELIVERY " + clientName[num] + " " + message;
								const char *buf = delivery.c_str();
								// Delivery via TCP
								if (j < maxClients) {
									bytesSent = send(clientSocket[j], buf, (int)delivery.size(), 0);
									getpeername(clientSocket[j], (sockaddr *)&recipient, &addrLength);
								}
								// Delivery via UDP
								else {
									bytesSent = sendto(serverSocketUDP, buf, (int)delivery.size(), 0, (sockaddr*)&clientAddr[j - maxClients], addrLength);
									recipient = clientAddr[j - maxClients];
								}
								// Logging
								std::cout << inet_ntoa(recipient.sin_addr) << ":" << ntohs(recipient.sin_port);
								if (bytesSent == SOCKET_ERROR) {
									std::cout << " <- Error delivering message, code:" << WSAGetLastError() << std::endl;
									response = SERVER_ERROR;
								}
								else {
									std::cout << " <- " << delivery;
									response = SEND_OK;
								}
							}
							else {
								response = UNKNOWN;
							}
						}
					}
					else {
						response = BAD_RQST_HDR;
					}
				}
				else {
					response = BAD_RQST_HDR;
				}
				// Send reply
				const char *buf = response.c_str();
				bytesSent = sendto(serverSocketUDP, buf, (int)response.size(), 0, (sockaddr*)&client, addrLength);
				std::cout << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port);
				if (bytesSent == SOCKET_ERROR) {
					std::cout << " <- Error sending message, code:" << WSAGetLastError() << std::endl;
				}
				else {
					std::cout << " <- " << response;
				}
			}
		}
		// Something happened on a Client Socket (TCP)
		else {
			// Loop through all client sockets
			for (int num = 0; num < maxClients; num++) {
				// Check if it is in use
				if (clientSocket[num] == 0) {
					continue;
				}
				temp = clientSocket[num];
				// Check if something happened
				if (FD_ISSET(temp, &readfds)) {
					// Logging
					getpeername(temp, (sockaddr*)&client, &addrLength);
					std::cout << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port);
					// Receive message
					bytes = recv(temp, buffer, bufSize, 0);
					// Something went wrong
					if (bytes == SOCKET_ERROR) {
						// Check if they abruptly disconnected
						if (WSAGetLastError() == WSAECONNRESET) {
							std::cout << " -> Unexpectedly Disconnected." << std::endl;
							closesocket(temp);
							clientSocket[num] = 0;
							clientName[num] = "";
						}
						else {
							std::cout << " -> Failed Receiving. Code: " << WSAGetLastError() << std::endl;
						}
					}
					// Check if they disconnected
					else if (bytes == 0) {
						std::cout << " -> Disconnected." << std::endl;

						closesocket(temp);
						clientSocket[num] = 0;
						clientName[num] = "";
					}
					// Process the message
					else {
						buffer[bytes] = '\0';
						std::string msg = buffer;
						std::cout << " -> " << msg;
						// Handshake
						if (msg.size() > 10 && msg.substr(0, 10) == "HELLO-FROM") {
							if (msg.size() > 11) {
								// Check if they already logged in
								if (!clientName[num].empty()) {
									response = BAD_RQST_HDR;
								}
								else {
									// Extract name from message
									std::string name;
									int index = 11;
									while (index < msg.size() && !isspace(msg.at(index))) {
										name += msg.at(index);
										index++;
									}
									// Check if it is already being used
									bool inuse = false;
									if (name == "echobot") {
										inuse = true;
									}
									else {
										for (int i = 0; i < 2 * maxClients; i++) {
											if (name == clientName[i]) {
												inuse = true;
												break;
											}
										}
									}
									// Set appropriate response
									if (inuse) {
										response = IN_USE;
									}
									else {
										response = "HELLO " + name + "\n";
										clientName[num] = name;
									}
								}
							}
							// No name
							else {
								response = BAD_RQST_BODY;
							}
						}
						// Online users
						else if (msg == "WHO\n") {
							// Check if they already logged in
							if (clientName[num].empty()) {
								response = BAD_RQST_HDR;
							}
							else {
								// Loop through all names
								response = "WHO-OK echobot";
								for (int i = 0; i < 2 * maxClients; i++) {
									if (!clientName[i].empty()) {
										response += ",";
										response += clientName[i];
									}
								}
								response += "\n";
							}
						}
						// Send a message
						else if (msg.size() > 4 && msg.substr(0, 4) == "SEND") {
							// Check if they already logged in
							if (clientName[num].empty()) {
								response = BAD_RQST_HDR;
							}
							else {
								// Extract user and message from msg
								std::string user, message;
								unsigned long long int i = 5;
								while (i < msg.size() && msg.at(i) != ' ') {
									user += msg.at(i);
									i++;
								}
								while (i < msg.size() - 1) {
									i++;
									message += msg.at(i);
								}
								// Invalid user or message
								if (user.empty() || message.empty()) {
									response = BAD_RQST_BODY;
								}
								// Message to self or echobot
								else if (user == "echobot" || user == clientName[num]) {
									bytesSent = send(temp, SEND_OK, (int)strlen(SEND_OK), 0);
									std::cout << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port);
									if (bytesSent == SOCKET_ERROR) {
										std::cout << " <- Error sending message, code:" << WSAGetLastError() << std::endl;
									}
									else {
										std::cout << " <- SEND-OK" << std::endl;
									}
									response = "DELIVERY " + user + " " + message;
								}
								// Delivery to someone
								else {
									// Check if recipient exists
									bool inuse = false;
									int j;
									for (j = 0; j < 2 * maxClients; j++) {
										if (clientName[j] == user) {
											inuse = true;
											break;
										}
									}
									if (inuse) {
										std::string delivery = "DELIVERY " + clientName[num] + " " + message;
										const char *buf = delivery.c_str();
										// Message to TCP
										if (j < maxClients) {
											bytesSent = send(clientSocket[j], buf, (int)delivery.size(), 0);
											getpeername(clientSocket[j], (sockaddr *)&recipient, &addrLength);
										}
										// Message to UDP
										else {
											bytesSent = sendto(serverSocketUDP, buf, (int)delivery.size(), 0, (sockaddr*)&clientAddr[j - maxClients], addrLength);
											recipient = clientAddr[j - maxClients];
										}
										// Logging
										std::cout << inet_ntoa(recipient.sin_addr) << ":" << ntohs(recipient.sin_port);
										if (bytesSent == SOCKET_ERROR) {
											std::cout << " <- Error delivering message, code:" << WSAGetLastError() << std::endl;
											response = SERVER_ERROR;
										}
										else {
											std::cout << " <- " << delivery;
											response = SEND_OK;
										}
									}
									// Recipient unknown
									else {
										response = UNKNOWN;
									}
								}
							}
						}
						else {
							response = BAD_RQST_HDR;
						}

						// Send response
						const char *buf = response.c_str();
						bytesSent = send(temp, buf, (int)response.size(), 0);
						std::cout << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port);
						if (bytesSent == SOCKET_ERROR) {
							std::cout << " <- Error sending message, code:" << WSAGetLastError() << std::endl;
						}
						else {
							std::cout << " <- " << response;
						}

						if (response == IN_USE) {
							closesocket(temp);
							clientSocket[num] = 0;
						}
					} // received bytes is an actual message
				} // clientSocket[num] is set
			} // loop through clientSocket
		} // serverSockets are not set
	} // loop until stop

	// Cleaning up
	for (int i = 0; i < maxClients; i++) {
		if (clientSocket[i] > 0) {
			closesocket(clientSocket[i]);
		}
	}
	closesocket(serverSocketTCP);
	closesocket(serverSocketUDP);
	FD_ZERO(&readfds);

	delete[] buffer;
	WSACleanup();
	readThread.join();

	std::cout << " > Shut down." << std::endl;

	return 0;
}