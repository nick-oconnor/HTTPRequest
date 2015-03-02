/*
  HTTPRequest.h - Library for processing HTTP requests.
  Created by Nicholas Pitt, February 26, 2015.
  Released under the LGPL license.
*/
#ifndef HTTPRequest_H
#define HTTPRequest_H

#include "Arduino.h"
#include "Client.h"

enum Status
{
	OK,
	NOT_FOUND,
	NOT_IMPLEMENTED
};

enum Method
{
	GET,
	POST,
	HEAD,
	UNKNOWN
};

class HTTPRequest
{
	public:
		HTTPRequest(Client &);
		Method getMethod() { return method; };
		String getURI() { return URI; };
		String getData() { return data; };
		String getValue(String);
		void response(Status, int);

	private:
		Client &client;
		Method method = UNKNOWN;
		String URI;
		String data;
};

#endif