/*
  HTTPRequest.cpp - Library for processing HTTP requests.
  Created by Nicholas Pitt, February 26, 2015.
  Released under the LGPL license.
*/

#include "HTTPRequest.h"

HTTPRequest::HTTPRequest(Client &client) : client(client)
{
	bool methodHeader = true;
	int pos, contentLength = 0;
	char buffer;
	String header, methodString;

	while (client.connected())
	{
		if (client.available())
		{
			buffer = client.read();
			if (buffer != '\n')
			{
				header += buffer;
			}
			else
			{
				if (methodHeader)
				{
					methodString = header.substring(0, header.indexOf(' '));
					if (methodString == "GET")
					{
						method = GET;
					}
					else if (methodString == "POST")
					{
						method = POST;
					}
					else if (methodString == "HEAD")
					{
						method = HEAD;
					}
					pos = header.indexOf('/');
					URI = header.substring(pos, header.indexOf(' ', pos + 1));
					pos = URI.indexOf('?');
					if (pos != -1)
					{
						data = URI.substring(pos + 1);
						URI = URI.substring(0, pos);
					}
					methodHeader = false;
				}
				else if (header.indexOf("Content-Length: ") == 0)
				{
					contentLength = header.substring(header.indexOf(' ') + 1).toInt();
				}
				else if (header.length() == 1)
				{
					if (contentLength)
					{
						while (client.connected())
						{
							if (client.available())
							{
								data += (char)client.read();
								contentLength--;
								if (!contentLength)
								{
									break;
								}
							}
						}
					}
					break;
				}
				header = "";
			}
		}
	}
}

String HTTPRequest::getValue(String name)
{
	int pos;

	pos = data.indexOf(name + '=');
	if (pos != -1)
	{
		data = data.substring(data.indexOf('=') + 1);
		pos = data.indexOf('&');
		if (pos != -1)
		{
			return data.substring(0, pos);
		}
		return data;
	}
	return "";
}

void HTTPRequest::response(Status status, int length)
{
	if (client.connected())
	{
		client.print("HTTP/1.1 ");
		if (status == OK)
		{
			client.println("200 OK");
		}
		else if (status == NOT_FOUND)
		{
			client.println("404 Not Found");
		}
		else if (status == NOT_IMPLEMENTED)
		{
			client.println("501 Not Implemented");
		}
		client.println("Connection: close");
		client.print("Content-Length: ");
		client.println(length);
		client.println("Content-Type: text/html");
		client.println();
	}
}