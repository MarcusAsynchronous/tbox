/*!The Treasure Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2011, ruki All rights reserved.
 *
 * \author		ruki
 * \file		eobject.c
 *
 */
/* /////////////////////////////////////////////////////////
 * includes
 */
#include "prefix.h"
#include "../../../event/eobject.h"
#include <sys/types.h>
#include <sys/socket.h>
#if defined(TB_CONFIG_EVENT_HAVE_POLL)
# 	include <sys/poll.h>
#elif defined(TB_CONFIG_EVENT_HAVE_SELECT)
# 	include <sys/select.h>
#endif

/* /////////////////////////////////////////////////////////
 * implemention
 */

#if defined(TB_CONFIG_EVENT_HAVE_POLL)
tb_long_t tb_eobject_wait_impl(tb_eobject_t* object, tb_long_t timeout)
{
	tb_assert_and_check_return_val(object, -1);

	// type
	tb_size_t otype = object->otype;
	tb_size_t etype = object->etype;
	tb_assert_and_check_return_val(otype == TB_EOTYPE_FILE || otype == TB_EOTYPE_SOCK, -1);

	// fd
	tb_long_t fd = ((tb_long_t)object->handle) - 1;
	tb_assert_and_check_return_val(fd >= 0, -1);
	
	// init
	struct pollfd pfd = {0};
	pfd.fd = fd;
	if (etype & TB_ETYPE_READ || etype & TB_ETYPE_ACPT) pfd.events |= POLLIN;
	if (etype & TB_ETYPE_WRIT || etype & TB_ETYPE_CONN) pfd.events |= POLLOUT;

	// poll
	tb_long_t r = poll(&pfd, 1, timeout);
	tb_assert_and_check_return_val(r >= 0, -1);

	// timeout?
	tb_check_return_val(r, 0);

	// closed?
	tb_check_return_val(!(pfd.revents & POLLHUP), -1);

	// error?
	if (otype == TB_EOTYPE_SOCK)
	{
		tb_int_t o = 0;
		tb_int_t n = sizeof(tb_int_t);
		getsockopt(fd, SOL_SOCKET, SO_ERROR, &o, &n);
		if (o) return -1;
	}

	// ok
	tb_long_t e = 0;
	if (pfd.revents & POLLIN) 
	{
		e |= TB_ETYPE_READ;
		if (etype & TB_ETYPE_ACPT) e |= TB_ETYPE_ACPT;
	}
	if (pfd.revents & POLLOUT) 
	{
		e |= TB_ETYPE_WRIT;
		if (etype & TB_ETYPE_CONN) e |= TB_ETYPE_CONN;
	}
	return e;
}

#elif defined(TB_CONFIG_EVENT_HAVE_SELECT)
tb_long_t tb_eobject_wait_impl(tb_eobject_t* object, tb_long_t timeout)
{
	tb_assert_and_check_return_val(object, -1);

	// type
	tb_size_t otype = object->otype;
	tb_size_t etype = object->etype;
	tb_assert_and_check_return_val(otype == TB_EOTYPE_FILE || otype == TB_EOTYPE_SOCK, -1);

	// fd
	tb_long_t fd = ((tb_long_t)object->handle) - 1;
	tb_assert_and_check_return_val(fd >= 0, -1);
	
	// init time
	struct timeval t = {0};
	if (timeout > 0)
	{
		t.tv_sec = timeout / 1000;
		t.tv_usec = (timeout % 1000) * 1000;
	}

	// init fds
	fd_set 	rfds;
	fd_set 	wfds;
	fd_set 	efds;
	fd_set* prfds = (etype & TB_ETYPE_READ || etype & TB_ETYPE_ACPT)? &rfds : TB_NULL;
	fd_set* pwfds = (etype & TB_ETYPE_WRIT || etype & TB_ETYPE_CONN)? &wfds : TB_NULL;

	if (prfds)
	{
		FD_ZERO(prfds);
		FD_SET(fd, prfds);
	}

	if (pwfds)
	{
		FD_ZERO(pwfds);
		FD_SET(fd, pwfds);
	}
	
	FD_ZERO(&efds);
	FD_SET(fd, &efds);

	// select
	tb_long_t r = select(fd + 1
						, prfds
						, pwfds
						, &efds
						, timeout >= 0? &t : TB_NULL);
	tb_assert_and_check_return_val(r >= 0, -1);

	// timeout?
	tb_check_return_val(r, 0);

	// closed?
	tb_check_return_val(!FD_ISSET(fd, &efds), -1);

	// error?
	if (otype == TB_EOTYPE_SOCK)
	{
		tb_int_t o = 0;
		tb_int_t n = sizeof(tb_int_t);
		getsockopt(fd, SOL_SOCKET, SO_ERROR, &o, &n);
		if (o) return -1;
	}

	// ok
	tb_long_t e = 0;
	if (prfds && FD_ISSET(fd, &rfds)) 
	{
		e |= TB_ETYPE_READ;
		if (etype & TB_ETYPE_ACPT) e |= TB_ETYPE_ACPT;
	}
	if (pwfds && FD_ISSET(fd, &wfds)) 
	{
		e |= TB_ETYPE_WRIT;
		if (etype & TB_ETYPE_CONN) e |= TB_ETYPE_CONN;
	}
	return e;
}
#else
# 	error have not available event mode
#endif