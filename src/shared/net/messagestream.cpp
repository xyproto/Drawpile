/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include <QDebug>

#include "messagestream.h"
#include "snapshot.h"
#include "undo.h"

namespace protocol {

MessageStream::MessageStream()
	: _offset(0), _snapshotpointer(-1), _bytes(0)
{
}

void MessageStream::append(MessagePtr msg)
{
	_messages.append(msg);
	_bytes += msg->length();
}

void MessageStream::addSnapshotPoint()
{
	// Sanity checking
	if(hasSnapshot()) {
		const SnapshotPoint &sp = snapshotPoint().cast<protocol::SnapshotPoint>();
		if(!sp.isComplete()) {
			qWarning() << "Tried to add a new snapshot point even though the old one isn't finished!";
			return;
		}
	}

	// Create the new point
	append(MessagePtr(new SnapshotPoint()));
	_snapshotpointer = end()-1;
}

int MessageStream::cleanup()
{
	if(hasSnapshot()) {
		const SnapshotPoint &sp = snapshotPoint().cast<protocol::SnapshotPoint>();
		if(sp.isComplete()) {
			int i = _snapshotpointer - _offset;
			Q_ASSERT(i>=0);
			if(i>0) {
				_messages = _messages.mid(i);
				_offset += i;
				_snapshotpointer = _offset;

				uint newlen=0;
				for(int j=1;j<_messages.count();++j)
					newlen += _messages.at(j)->length();
				_bytes = newlen;

			}
			return i;
		}
	}
	return 0;
}

void MessageStream::hardCleanup(uint sizelimit)
{
	// First, find the index of the last protected undo point
	int undo_point = _offset;
	int undo_points = 0;
	for(int i=end()-1;i>=offset() && undo_points<UNDO_HISTORY_LIMIT;--i) {
		if(at(i)->type() == MSG_UNDOPOINT) {
			undo_point = i;
			++undo_points;
		}
	}

	// Remove messages until size limit or protected undo point is reached
	while(_bytes > sizelimit && _offset < undo_point) {
		_bytes -= _messages.takeFirst()->length();
		++_offset;
	}
}

void MessageStream::clear()
{
	_offset = end();
	_snapshotpointer = -1;
	_messages.clear();
	_bytes = 0;
}

QList<MessagePtr> MessageStream::toCommandList() const
{
	QList<MessagePtr> lst;
	foreach(MessagePtr m, _messages)
		if(m->isCommand())
			lst.append(m);
	return lst;
}

}

