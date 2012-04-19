/*  This file is part of the Tufão project
    Copyright (C) 2011 Vinícius dos Santos Oliveira <vini.ipsmaker@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any
    later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TUFAO_HEADERS_H
#define TUFAO_HEADERS_H

#include <QtCore/QMultiMap>
#include "ibytearray.h"

class QDateTime;

namespace Tufao {

/*!
  This class provides a representation of HTTP headers.

  HTTP headers are string-based properties with case-insensitive keys.

  \sa
  Tufao::IByteArray
  */
struct TUFAO_EXPORT Headers: public QMultiMap<IByteArray, QByteArray>
{
    /*!
      \since 0.3
      */
    static QByteArray fromDateTime(const QDateTime &dateTime);

    /*!
      \since 0.3
      */
    static QDateTime toDateTime(const QByteArray &headerValue);
};

} // namespace Tufao

#endif // TUFAO_HEADERS_H
