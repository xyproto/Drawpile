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

#include <QtEndian>

#include "annotation.h"

namespace protocol {

AnnotationCreate *AnnotationCreate::deserialize(const uchar *data, uint len)
{
	if(len!=14)
		return 0;
	return new AnnotationCreate(
		*(data+0),
		*(data+1),
		qFromBigEndian<qint32>(data+2),
		qFromBigEndian<qint32>(data+6),
		qFromBigEndian<quint16>(data+10),
		qFromBigEndian<quint16>(data+12)
	);
}

int AnnotationCreate::payloadLength() const
{
	return 1 + 1 + 4*2 + 2*2;
}

int AnnotationCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 2;
	qToBigEndian(_h, ptr); ptr += 2;
	return ptr - data;
}

int AnnotationReshape::payloadLength() const
{
	return 1 + 1 + 4*2 + 2*2;
}

AnnotationReshape *AnnotationReshape::deserialize(const uchar *data, uint len)
{
	if(len!=14)
		return 0;
	return new AnnotationReshape(
		*(data+0),
		*(data+1),
		qFromBigEndian<qint32>(data+2),
		qFromBigEndian<qint32>(data+6),
		qFromBigEndian<quint16>(data+10),
		qFromBigEndian<quint16>(data+12)
	);
}

int AnnotationReshape::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 2;
	qToBigEndian(_h, ptr); ptr += 2;
	return ptr - data;
}

AnnotationEdit *AnnotationEdit::deserialize(const uchar *data, uint len)
{
	if(len < 6)
		return 0;

	return new AnnotationEdit(
		*data,
		*(data+1),
		qFromBigEndian<quint32>(data+2),
		QByteArray((const char*)data+6, len-6)
	);
}

int AnnotationEdit::payloadLength() const
{
	return 1 + 1 + 4 + _text.length();
}

int AnnotationEdit::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	qToBigEndian(_bg, ptr); ptr += 4;
	memcpy(ptr, _text.constData(), _text.length());
	ptr += _text.length();
	return ptr - data;
}

AnnotationDelete *AnnotationDelete::deserialize(const uchar *data, uint len)
{
	if(len != 2)
		return 0;
	return new AnnotationDelete(data[0], data[1]);
}

int AnnotationDelete::payloadLength() const
{
	return 2;
}

int AnnotationDelete::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	return ptr-data;
}

}
