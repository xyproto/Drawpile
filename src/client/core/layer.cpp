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
#include <QPainter>
#include <QImage>
#include <cmath>

#include "layerstack.h"
#include "layer.h"
#include "tile.h"
#include "brush.h"
#include "brushmask.h"
#include "point.h"

namespace paintcore {

namespace {

//! Sample colors at layer edges and return the most frequent color
QColor _sampleEdgeColors(const Layer *layer, bool top, bool right, bool bottom, bool left)
{
	const int STEP = 22;
	QHash<QRgb, int> colorfreq;
	// Sample top & bottom edges
	for(int x=0;x<layer->width();x+=STEP) {
		int count;
		QRgb c;
		if(top) {
			c = layer->pixelAt(x, 0);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}

		if(bottom) {
			c = layer->pixelAt(x, layer->height()-1);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}
	}

	// Sample left & right edges
	for(int y=0;y<layer->height();y+=STEP) {
		int count;
		QRgb c;
		if(left) {
			c = layer->pixelAt(0, y);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}

		if(right) {
			c = layer->pixelAt(layer->width()-1, y);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}
	}

	// Return the most frequent color
	QRgb color=0;
	int freq=0;
	QHashIterator<QRgb, int> i(colorfreq);
	while(i.hasNext()) {
		i.next();
		if(i.value() > freq) {
			freq = i.value();
			color = i.key();
		}
	}

	return QColor::fromRgba(color);
}

}

/**
 * Construct a layer initialized to a solid color
 * @param owner the stack to which this layer belongs to
 * @param id layer ID
 * @param color layer color
 * @parma size layer size
 */
Layer::Layer(LayerStack *owner, int id, const QString& title, const QColor& color, const QSize& size)
	: owner_(owner), id_(id), _title(title), _width(0), _height(0), _xtiles(0), _ytiles(0),
	_opacity(255), _blend(1), _hidden(false)
{
	resize(0, size.width(), size.height(), 0);
	
	if(color.alpha() > 0) {
		for(int i=0;i<_tiles.size();++i)
			_tiles[i].fillColor(color);
	}
}

Layer::Layer(LayerStack *owner, int id, const QSize &size)
	: Layer(owner, id, "", Qt::transparent, size)
{
	// sublayers are used for indirect drawing
}

Layer::Layer(const Layer &layer)
	: owner_(layer.owner_), id_(layer.id()), _title(layer._title),
	  _width(layer._width), _height(layer._height),
	  _xtiles(layer._xtiles), _ytiles(layer._ytiles),
	  _tiles(layer._tiles),
	  _opacity(layer._opacity), _blend(layer._blend), _hidden(layer._hidden)
{
	foreach(const Layer *sl, layer._sublayers)
		_sublayers.append(new Layer(*sl));
}

Layer::~Layer() {
	foreach(Layer *sl, _sublayers)
		delete sl;
}

void Layer::resize(int top, int right, int bottom, int left)
{
	// Minimize amount of data that needs to be copied
	optimize();

	// Resize sublayers
	foreach(Layer *sl, _sublayers)
		sl->resize(top, right, bottom, left);

	// Calculate new size
	int width = left + _width + right;
	int height = top + _height + bottom;

	int xtiles = Tile::roundTiles(width);
	int ytiles = Tile::roundTiles(height);
	QVector<Tile> tiles(xtiles * ytiles);

	// if there is no old content, resizing is simple
	bool hascontent = false;
	for(int i=0;i<_tiles.count();++i) {
		if(!_tiles.at(i).isBlank()) {
			hascontent = true;
			break;
		}
	}
	if(!hascontent) {
		_width = width;
		_height = height;
		_xtiles = xtiles;
		_ytiles = ytiles;
		_tiles = tiles;
		return;
	}

	// Sample colors around the layer edges to determine fill color
	// for the new tiles
	QColor bgcolor = _sampleEdgeColors(this, top>0, right>0, bottom>0, left>0);

	if((left % Tile::SIZE) || (top % Tile::SIZE)) {
		// If top/left adjustment is not divisble by tile size,
		// we need to move the layer content

		QImage oldcontent = toImage();

		_width = width;
		_height = height;
		_xtiles = xtiles;
		_ytiles = ytiles;
		_tiles = tiles;
		if(left<0 || top<0) {
			int cropx = 0;
			if(left<0) {
				cropx = -left;
				left = 0;
			}
			int cropy = 0;
			if(top<0) {
				cropy = -top;
				top = 0;
			}
			oldcontent = oldcontent.copy(cropx, cropy, oldcontent.width()-cropx, oldcontent.height()-cropy);
		}

		if(bgcolor.alpha()>0)
			fillColor(bgcolor);

		putImage(left, top, oldcontent, false);
	} else {
		// top/left offset is aligned at tile boundary:
		// existing tile content can be reused

		const int firstrow = Tile::roundTiles(-top);
		const int firstcol = Tile::roundTiles(-left);

		int oldy = firstrow;
		for(int y=0;y<ytiles;++y,++oldy) {
			int oldx = firstcol;
			const int oldyy = _xtiles * oldy;
			const int yy = xtiles * y;
			for(int x=0;x<xtiles;++x,++oldx) {
				const int i = yy + x;

				if(oldy<0 || oldy>=_ytiles || oldx<0 || oldx>=_xtiles) {
					if(bgcolor.alpha()>0)
						tiles[i].fillColor(bgcolor);
				} else {
					const int oldi = oldyy + oldx;
					tiles[i] = _tiles.at(oldi);
				}
			}
		}

		_width = width;
		_height = height;
		_xtiles = xtiles;
		_ytiles = ytiles;
		_tiles = tiles;
	}
}

void Layer::setTitle(const QString& title)
{
	_title = title;
}

QImage Layer::toImage() const {
	QImage image(_width, _height, QImage::Format_ARGB32);
	int i=0;
	for(int y=0;y<_ytiles;++y) {
		for(int x=0;x<_xtiles;++x,++i)
			_tiles[i].copyToImage(image, x*Tile::SIZE, y*Tile::SIZE);
	}
	return image;
}

/**
 * @param x
 * @param y
 * @return invalid color if x or y is outside image boundaries
 */
QColor Layer::colorAt(int x, int y) const
{
	if(x<0 || y<0 || x>=_width || y>=_height)
		return QColor();

	return QColor::fromRgb(pixelAt(x, y));
}

QRgb Layer::pixelAt(int x, int y) const
{
	if(x<0 || y<0 || x>=_width || y>=_height)
		return 0;

	const int yindex = y/Tile::SIZE;
	const int xindex = x/Tile::SIZE;

	return tile(xindex, yindex).pixel(x-xindex*Tile::SIZE, y-yindex*Tile::SIZE);
}

/**
 * @param opacity
 */
void Layer::setOpacity(int opacity)
{
	Q_ASSERT(opacity>=0 && opacity<256);
	_opacity = opacity;
	// TODO optimization: mark only nonempty tiles
	if(owner_ && visible())
		owner_->markDirty();
}

void Layer::setBlend(int blend)
{
	_blend = blend;
	// TODO optimization: mark only nonempty tiles
	if(owner_ && visible())
		owner_->markDirty();
}

/**
 * @param hide new status
 */
void Layer::setHidden(bool hide)
{
	_hidden = hide;
	// TODO same optimization as above
	if(owner_ && _opacity>0)
		owner_->markDirty();
}

/**
 * Return a copy of the image with the borders padded to align with tile boundaries.
 * The padding pixels are taken from the layer content, so the image can be used
 * to replace the existing tiles.
 * @param xpos target image position
 * @param ypos target image position
 * @param original the image to pad
 * @param alpha alpha blend image
 */
QImage Layer::padImageToTileBoundary(int xpos, int ypos, const QImage &original, bool alpha) const
{
	const int x0 = Tile::roundDown(xpos);
	const int x1 = qMin(_width, Tile::roundUp(xpos+original.width()));
	const int y0 = Tile::roundDown(ypos);
	const int y1 = qMin(_height, Tile::roundUp(ypos+original.height()));

	const int w = x1 - x0;
	const int h = y1 - y0;
	
	QImage image(w, h, QImage::Format_ARGB32);
	//image.fill(0);

	// Copy background from existing tiles
	for(int y=0;y<h;y+=Tile::SIZE) {
		//int yt = (y0 + y) / Tile::SIZE;
		for(int x=0;x<w;x+=Tile::SIZE) {
			const Tile &t = tile((x0+x) / Tile::SIZE, (y0+y) / Tile::SIZE);
			t.copyToImage(image, x, y);
		}
	}
	
	// Paint new image
	QPainter painter(&image);
	if(!alpha)
		painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.drawImage(xpos-x0, ypos-y0, original);
	
#if 0 /* debugging aid */
	painter.setPen(Qt::red);
	painter.drawLine(0, 0, w-1, 0);
	painter.drawLine(0, 0, 0, h-1);
	painter.drawLine(w-1, 0, w-1, h-1);
	painter.drawLine(0, h-1, w-1, h-1);
#endif
	
	return image;
}

/**
 * @param x x coordinate
 * @param y y coordinate
 * @param image the image to draw
 * @param blend use alpha blending
 */
void Layer::putImage(int x, int y, QImage image, bool blend)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32);

	// Protocol uses unsigned coordinates, so we don't need to support
	// negative values here either.
	Q_ASSERT(x>=0 && y>=0);
	if(x<0 || y<0)
		return;
	
	// Check if the image is completely outside the layer
	if(x >= _width || y >= _height)
		return;

	const int x0 = Tile::roundDown(x);
	const int y0 = Tile::roundDown(y);
	
	if(x-x0 || y-y0 || image.width() % Tile::SIZE || image.height() % Tile::SIZE || blend) {
		image = padImageToTileBoundary(x, y, image, blend);
	}
	
	const int tx0 = x0 / Tile::SIZE;
	const int ty0 = y0 / Tile::SIZE;
	const int tx1 = qMin((x0 + image.width() - 1) / Tile::SIZE, _xtiles-1);
	const int ty1 = qMin((y0 + image.height() - 1) / Tile::SIZE, _ytiles-1);

	for(int ty=ty0;ty<=ty1;++ty) {
		int yoff = (ty-ty0) * Tile::SIZE;
		for(int tx=tx0;tx<=tx1;++tx) {
			int xoff = (tx-tx0) * Tile::SIZE;
			int i = ty*_xtiles + tx;
			Q_ASSERT(i>=0 && i < _xtiles*_ytiles);
			_tiles[i] = Tile(image, xoff, yoff);
		}
	}
	
	if(owner_ && visible())
		owner_->markDirty(QRect(x, y, image.width(), image.height()));
}

void Layer::dab(int contextId, const Brush &brush, const Point &point)
{
	if(!brush.incremental()) {
		// Indirect brush: use a sublayer
		Layer *sl = getSubLayer(contextId, brush.blendingMode(), brush.opacity(1) * 255);

		Brush slb(brush);
		slb.setOpacity(1.0);
		slb.setOpacity2(brush.isOpacityVariable() ? 0.0 : 1.0);
		slb.setBlendingMode(1);

		sl->directDab(slb, BrushMaskGenerator::cached(slb), point);
	} else {
		directDab(brush, BrushMaskGenerator::cached(brush), point);
	}
}

/**
 * Draw a line using either drawSoftLine or drawHardLine, depending on
 * the subpixel hint of the brush.
 * @param context drawing context id (needed for indirect drawing)
 */
void Layer::drawLine(int contextId, const Brush& brush, const Point& from, const Point& to, qreal &distance)
{
	if(!brush.incremental()) {
		// Indirect brush: use a sublayer
		Layer *sl = getSubLayer(contextId, brush.blendingMode(), brush.opacity(1) * 255);

		Brush slb(brush);
		slb.setOpacity(1.0);
		slb.setOpacity2(brush.isOpacityVariable() ? 0.0 : 1.0);
		slb.setBlendingMode(1);

		const BrushMaskGenerator &bmg = BrushMaskGenerator::cached(slb);

		if(brush.subpixel())
			sl->drawSoftLine(slb, bmg, from, to, distance);
		else
			sl->drawHardLine(slb, bmg, from, to, distance);
	} else {
		const BrushMaskGenerator &bmg = BrushMaskGenerator::cached(brush);

		if(brush.subpixel())
			drawSoftLine(brush, bmg, from, to, distance);
		else
			drawHardLine(brush, bmg, from, to, distance);
	}
}

/**
 * This function is optimized for drawing with subpixel precision.
 * @param brush brush to draw the line with
 * @param from starting point
 * @param to ending point
 * @param distance distance from previous dab.
 */
void Layer::drawSoftLine(const Brush& brush, const BrushMaskGenerator &mask, const Point& from, const Point& to, qreal &distance)
{
	const qreal spacing = brush.spacing()*brush.radius(from.pressure())/100.0;
	qreal dx = to.x() - from.x();
	qreal dy = to.y() - from.y();
	const qreal dist = hypot(dx, dy);
	dx = dx / dist;
	dy = dy / dist;
	const qreal dp = (to.pressure() - from.pressure()) / dist;

	// Skip the first dab.
	Point p(from.x() + dx, from.y()+dy, qBound(0.0, from.pressure() + dp, 1.0));

	for(qreal i=0;i<dist-0.5;++i) {
		if(++distance > spacing) {
			directDab(brush, mask, p);
			distance = 0;
		}
		p.rx() += dx;
		p.ry() += dy;
		p.setPressure(qBound(0.0, p.pressure() + dp, 1.0));
	}
}

/**
 * This line drawing function is optimized for drawing with no subpixel
 * precision.
 * The last point is not drawn, so successive lines can be drawn blotches.
 */
void Layer::drawHardLine(const Brush &brush, const BrushMaskGenerator& mask, const Point& from, const Point& to, qreal &distance) {
	const qreal dp = (to.pressure()-from.pressure()) / hypot(to.x()-from.x(), to.y()-from.y());

	const int spacing = brush.spacing()*brush.radius(from.pressure())/100;

	int x0 = from.x();
	int y0 = from.y();
	qreal p = from.pressure();
	int x1 = to.x();
	int y1 = to.y();
	int dy = y1 - y0;
	int dx = x1 - x0;
	int stepx, stepy;

	if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else {
		stepy = 1;
	}
	if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else {
		stepx = 1;
	}

	dy *= 2;
	dx *= 2;

	if (dx > dy) {
		int fraction = dy - (dx >> 1);
		while (x0 != x1) {
			if (fraction >= 0) {
				y0 += stepy;
				fraction -= dx;
			}
			x0 += stepx;
			fraction += dy;
			if(++distance > spacing) {
				directDab(brush, mask, Point(x0, y0, p));
				distance = 0;
			}
			p += dp;
		}
	} else {
		int fraction = dx - (dy >> 1);
		while (y0 != y1) {
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;
			if(++distance > spacing) {
				directDab(brush, mask, Point(x0, y0, p));
				distance = 0;
			}
			p += dp;
		}
	}
}

/**
 * Apply a single dab of the brush to the layer
 * @param brush brush to use
 * @parma point where to dab. May be outside the image.
 */
void Layer::directDab(const Brush &brush, const BrushMaskGenerator& mask, const Point& point)
{
	const int dia = brush.diameter(point.pressure())+1; // space for subpixels
	const int top = point.y() - brush.radius(point.pressure());
	const int left = point.x() - brush.radius(point.pressure());
	const int bottom = qMin(top + dia, _height);
	const int right = qMin(left + dia, _width);
	if(left+dia<=0 || top+dia<=0 || left>=_width || top>=_height)
		return;

	// Render the brush
	BrushMask bm;
	if(brush.subpixel())
		bm = mask.make(point.xFrac(), point.yFrac(), point.pressure());
	else
		bm = mask.make(point.pressure());

	const int realdia = bm.diameter();
	const uchar *values = bm.data();
	QColor color = brush.color(point.pressure());

	// A single dab can (and often does) span multiple tiles.
	int y = top<0?0:top;
	int yb = top<0?-top:0; // y in relation to brush origin
	const int x0 = left<0?0:left;
	const int xb0 = left<0?-left:0;
	while(y<bottom) {
		const int yindex = y / Tile::SIZE;
		const int yt = y - yindex * Tile::SIZE;
		const int hb = yt+realdia-yb < Tile::SIZE ? realdia-yb : Tile::SIZE-yt;
		int x = x0;
		int xb = xb0; // x in relation to brush origin
		while(x<right) {
			const int xindex = x / Tile::SIZE;
			const int xt = x - xindex * Tile::SIZE;
			const int wb = xt+realdia-xb < Tile::SIZE ? realdia-xb : Tile::SIZE-xt;
			const int i = _xtiles * yindex + xindex;
			_tiles[i].composite(
					brush.blendingMode(),
					values + yb * realdia + xb,
					color,
					xt, yt,
					wb, hb,
					realdia-wb
					);

			x = (xindex+1) * Tile::SIZE;
			xb = xb + wb;
		}
		y = (yindex+1) * Tile::SIZE;
		yb = yb + hb;
	}
	if(owner_ && visible())
		owner_->markDirty(QRect(left, top, right-left, bottom-top));

}

/**
 * @param layer the layer that will be merged to this
 */
void Layer::merge(const Layer *layer)
{
	Q_ASSERT(layer->_xtiles == _xtiles);
	Q_ASSERT(layer->_ytiles == _ytiles);

	const bool md = owner_ && visible();

	for(int y=0;y<layer->_ytiles;++y) {
		for(int x=0;x<layer->_xtiles;++x) {
			const int index = _xtiles*y + x;

			bool merged;
			merged = _tiles[index].merge(layer->_tiles[index], layer->_opacity, layer->blendmode());
			if(md && merged)
				owner_->markDirty(x, y);
		}
	}
}

void Layer::fillChecker(const QColor& dark, const QColor& light)
{
	for(int i=0;i<_tiles.size();++i)
		_tiles[i].fillChecker(dark, light);
	if(owner_ && visible())
		owner_->markDirty();
}

void Layer::fillColor(const QColor& color)
{
	for(int i=0;i<_tiles.size();++i)
		_tiles[i].fillColor(color);
	if(owner_ && visible())
		owner_->markDirty();
}

/**
 * Free all tiles that are completely transparent
 */
void Layer::optimize()
{
	// Optimize tile memory usage
	for(int i=0;i<_tiles.size();++i)
		_tiles[i].optimize();

	// Delete unused sublayers
	QMutableListIterator<Layer*> li(_sublayers);
	while(li.hasNext()) {
		Layer *sl = li.next();
		if(sl->hidden()) {
			delete sl;
			li.remove();
		}
	}
}

void Layer::makeBlank()
{
	for(int i=0;i<_tiles.size();++i)
		_tiles[i].makeBlank();

	if(owner_ && visible())
		owner_->markDirty();
}

/**
 * @brief Get or create a new sublayer
 *
 * Sublayers are temporary layers used for indirect drawing.
 *
 * @param id sublayer ID (unique to parent layer only)
 * @param opacity layer opacity (set when creating the layer)
 * @return sublayer
 */
Layer *Layer::getSubLayer(int id, int blendmode, uchar opacity)
{
	// See if the sublayer exists already
	foreach(Layer *sl, _sublayers)
		if(sl->id() == id) {
			if(sl->hidden()) {
				// Hidden, reset properties
				sl->makeBlank();
				sl->_opacity = opacity;
				sl->_blend = blendmode;
				sl->_hidden = false;
			}
			return sl;
		}

	// Okay, try recycling a sublayer
	foreach(Layer *sl, _sublayers) {
		if(sl->hidden()) {
			// Set these flags directly to avoid markDirty call.
			// We know the layer is invisible at this point
			sl->makeBlank();
			sl->id_ = id;
			sl->_opacity = opacity;
			sl->_blend = blendmode;
			sl->_hidden = false;
			return sl;
		}
	}

	// No available sublayers, create a new one
	Layer *sl = new Layer(owner_, id, QSize(_width, _height));
	sl->_opacity = opacity;
	sl->_blend = blendmode;
	_sublayers.append(sl);
	return sl;
}

/**
 * This is used to end an indirect stroke.
 * If a sublayer with the given ID does not exist, this function does nothing.
 * @param id
 */
void Layer::mergeSublayer(int id)
{
	foreach(Layer *sl, _sublayers) {
		if(sl->id() == id) {
			if(!sl->hidden()) {
				merge(sl);
				// Set hidden flag directly to avoid markDirty call.
				// The merge should cause no visual change.
				sl->_hidden = true;
			}
			return;
		}
	}
}

}
