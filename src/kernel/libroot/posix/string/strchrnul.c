/* 
** Copyright 2003, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
** Distributed under the terms of the OpenBeOS License.
*/


#include <sys/types.h>
#include <string.h>


char *
strchrnul(const char *string, int c)
{
	while (string[0] != (char)c && string[0])
		string++;

	return (char *)string;
}

