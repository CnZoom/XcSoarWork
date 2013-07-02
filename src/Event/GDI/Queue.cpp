/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2013 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Queue.hpp"
#include "Event.hpp"
#include "Thread/Debug.hpp"

bool
EventQueue::Wait(Event &event)
{
  assert(InMainThread());

  while (true) {
    if (::PeekMessage(&event.msg, nullptr, 0, 0, PM_REMOVE))
      return event.msg.message != WM_QUIT;

    const DWORD n = 0;
    const LPHANDLE handles = nullptr;

    const DWORD timeout = INFINITE;
    DWORD result = ::MsgWaitForMultipleObjects(n, handles, false,
                                               timeout, QS_ALLEVENTS);
    if (result == 0xffffffff)
      return false;
  }
}

static void
HandleMessages(UINT wMsgFilterMin, UINT wMsgFilterMax)
{
  MSG msg;
  while (::PeekMessage(&msg, NULL, wMsgFilterMin, wMsgFilterMax, PM_REMOVE)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }
}

void
EventQueue::HandlePaintMessages()
{
  AssertNoneLocked();

  HandleMessages(WM_SIZE, WM_SIZE);
  HandleMessages(WM_PAINT, WM_PAINT);
}
