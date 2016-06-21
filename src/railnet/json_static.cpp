/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file json_static.cpp implementation of json classes
 */

#include "json_static.h"

void JsonIfileBase::ReadBool(bool& b, std::istream* is)
{
	char tmp[6];
	tmp[5]=tmp[4]=0;
	*is >> tmp[0];
	if(!ChkEnd(*tmp))
	{
		*is >> tmp[1];
		*is >> tmp[2];
		*is >> tmp[3];
		if(!strncmp(tmp, "true", 4))
			b = true;
		else
		{
			*is >> tmp[4];
			if(!strncmp(tmp, "false", 5))
				b = false;
			else throw "boolean must be 'true' or 'false'";
		}
	}
}

void JsonIfileBase::ReadString(std::string& s, std::istream* is)
{
	char tmp; *is >> tmp;
	if(!ChkEnd(tmp)) {
		assert_q(tmp);
		do { is->get(tmp); if(tmp != '\"') s.push_back(tmp); } while (tmp != '\"');
	}
}

void JsonIfileBase::ReadCStr(char* s, std::istream* is)
{
	char tmp; *is >> tmp;
	if(!ChkEnd(tmp)) {
		assert_q(tmp);
		do { is->get(tmp); if(tmp != '\"') *(s++) = tmp; } while (tmp != '\"');
		*s = 0;
	}
}

