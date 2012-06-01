/*
** Copyright (C) 2012 Fargier Sylvain <fargier.sylvain@free.fr>
**
** This software is provided 'as-is', without any express or implied
** warranty.  In no event will the authors be held liable for any damages
** arising from the use of this software.
**
** Permission is granted to anyone to use this software for any purpose,
** including commercial applications, and to alter it and redistribute it
** freely, subject to the following restrictions:
**
** 1. The origin of this software must not be misrepresented; you must not
**    claim that you wrote the original software. If you use this software
**    in a product, an acknowledgment in the product documentation would be
**    appreciated but is not required.
** 2. Altered source versions must be plainly marked as such, and must not be
**    misrepresented as being the original software.
** 3. This notice may not be removed or altered from any source distribution.
**
** redirect.h
**
**        Created on: Jun 01, 2012
**   Original Author: Fargier Sylvain <fargier.sylvain@free.fr>
**
*/

#ifndef __REDIRECT_H__
#define __REDIRECT_H__

int redirect_child(pid_t pid, const char *log_file);
#define __printf __attribute__((format(printf, 1, 2)))
void __printf die(const char *msg, ...);
void __printf debug(const char *msg, ...);
void __printf error(const char *msg, ...);

#endif

