#include "divetooltipitem.h"
#include "divecartesianaxis.h"
#include "profilewidget2.h"
#include "dive.h"
#include "profile.h"
#include "membuffer.h"
#include "metrics.h"
#include <QPropertyAnimation>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QBrush>
#include <QGraphicsScene>
#include <QSettings>
#include <QGraphicsView>
#include <QStyleOptionGraphicsItem>
#include <QDebug>

#define PORT_IN_PROGRESS 1
#ifdef PORT_IN_PROGRESS
#include "display.h"
#endif

void ToolTipItem::addToolTip(const QString &toolTip, const QIcon &icon, const QPixmap& pixmap)
{
	const IconMetrics& iconMetrics = defaultIconMetrics();

	QGraphicsPixmapItem *iconItem = 0;
	double yValue = title->boundingRect().height() + iconMetrics.spacing;
	Q_FOREACH (ToolTip t, toolTips) {
		yValue += t.second->boundingRect().height();
	}
	if (entryToolTip.second) {
		yValue += entryToolTip.second->boundingRect().height();
	}
	iconItem = new QGraphicsPixmapItem(this);
	if (!icon.isNull()) {
		iconItem->setPixmap(icon.pixmap(iconMetrics.sz_small, iconMetrics.sz_small));
	} else if (!pixmap.isNull()) {
		iconItem->setPixmap(pixmap);
	}
	iconItem->setPos(iconMetrics.spacing, yValue);

	QGraphicsSimpleTextItem *textItem = new QGraphicsSimpleTextItem(toolTip, this);
	textItem->setPos(iconMetrics.spacing + iconMetrics.sz_small + iconMetrics.spacing, yValue);
	textItem->setBrush(QBrush(Qt::white));
	textItem->setFlag(ItemIgnoresTransformations);
	toolTips.push_back(qMakePair(iconItem, textItem));
}

void ToolTipItem::clear()
{
	Q_FOREACH (ToolTip t, toolTips) {
		delete t.first;
		delete t.second;
	}
	toolTips.clear();
}

void ToolTipItem::setRect(const QRectF &r)
{
	if( r == rect() ) {
		return;
	}

	QGraphicsRectItem::setRect(r);
	updateTitlePosition();
}

void ToolTipItem::collapse()
{
	int dim = defaultIconMetrics().sz_small;

	QPropertyAnimation *animation = new QPropertyAnimation(this, "rect");
	animation->setDuration(100);
	animation->setStartValue(nextRectangle);
	animation->setEndValue(QRect(0, 0, dim, dim));
	animation->start(QAbstractAnimation::DeleteWhenStopped);
	clear();

	status = COLLAPSED;
}

void ToolTipItem::expand()
{
	if (!title)
		return;

	const IconMetrics& iconMetrics = defaultIconMetrics();

	double width = 0, height = title->boundingRect().height() + iconMetrics.spacing;
	Q_FOREACH (const ToolTip& t, toolTips) {
		if (t.second->boundingRect().width() > width)
			width = t.second->boundingRect().width();
		height += t.second->boundingRect().height();
	}

	if (entryToolTip.first) {
		if (entryToolTip.second->boundingRect().width() > width)
			width = entryToolTip.second->boundingRect().width();
		height += entryToolTip.second->boundingRect().height();
	}

	/*       Left padding, Icon Size,   space, right padding */
	width += iconMetrics.spacing + iconMetrics.sz_small + iconMetrics.spacing + iconMetrics.spacing;

	if (width < title->boundingRect().width() + iconMetrics.spacing * 2)
		width = title->boundingRect().width() + iconMetrics.spacing * 2;

	if (height < iconMetrics.sz_small)
		height = iconMetrics.sz_small;

	nextRectangle.setWidth(width);
	nextRectangle.setHeight(height);

	if (nextRectangle != rect()) {
		QPropertyAnimation *animation = new QPropertyAnimation(this, "rect", this);
		animation->setDuration(100);
		animation->setStartValue(rect());
		animation->setEndValue(nextRectangle);
		animation->start(QAbstractAnimation::DeleteWhenStopped);
	}

	status = EXPANDED;
}

ToolTipItem::ToolTipItem(QGraphicsItem *parent) : QGraphicsRectItem(parent),
	title(new QGraphicsSimpleTextItem(tr("Information"), this)),
	status(COLLAPSED),
	timeAxis(0),
	lastTime(-1)
{
	memset(&pInfo, 0, sizeof(pInfo));
	entryToolTip.first = NULL;
	entryToolTip.second = NULL;
	setFlags(ItemIgnoresTransformations | ItemIsMovable | ItemClipsChildrenToShape);

	QColor c = QColor(Qt::black);
	c.setAlpha(155);
	setBrush(c);
	setPen(QPen(QBrush(Qt::transparent), 0));

	setZValue(99);

	addToolTip(QString(), QIcon(), QPixmap(16,60));
	entryToolTip = toolTips.first();
	toolTips.clear();

	title->setFlag(ItemIgnoresTransformations);
	title->setPen(QPen(Qt::white, 1));
	title->setBrush(Qt::white);

	setBrush(QBrush(Qt::white));
	setPen(QPen(Qt::black, 0.5));
}

ToolTipItem::~ToolTipItem()
{
	clear();
}

void ToolTipItem::updateTitlePosition()
{
	const IconMetrics& iconMetrics = defaultIconMetrics();
	if (rect().width() < title->boundingRect().width() + iconMetrics.spacing * 4) {
		QRectF newRect = rect();
		newRect.setWidth(title->boundingRect().width() + iconMetrics.spacing * 4);
		newRect.setHeight((newRect.height() && isExpanded()) ? newRect.height() : iconMetrics.sz_small);
		setRect(newRect);
	}

	title->setPos(boundingRect().width() / 2 - title->boundingRect().width() / 2 - 1, 0);
}

bool ToolTipItem::isExpanded() const
{
	return status == EXPANDED;
}

void ToolTipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	persistPos();
	QGraphicsRectItem::mouseReleaseEvent(event);
	Q_FOREACH (QGraphicsItem *item, oldSelection) {
		item->setSelected(true);
	}
}

void ToolTipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(widget);
	painter->save();
	painter->setClipRect(option->rect);
	painter->setPen(pen());
	painter->setBrush(brush());
	painter->drawRoundedRect(rect(), 10, 10, Qt::AbsoluteSize);
	painter->restore();
}

void ToolTipItem::persistPos()
{
	QSettings s;
	s.beginGroup("ProfileMap");
	s.setValue("tooltip_position", pos());
	s.endGroup();
}

void ToolTipItem::readPos()
{
	QSettings s;
	s.beginGroup("ProfileMap");
	QPointF value = s.value("tooltip_position").toPoint();
	if (!scene()->sceneRect().contains(value)) {
		value = QPointF(0, 0);
	}
	setPos(value);
}

void ToolTipItem::setPlotInfo(const plot_info &plot)
{
	pInfo = plot;
}

void ToolTipItem::setTimeAxis(DiveCartesianAxis *axis)
{
	timeAxis = axis;
}

void ToolTipItem::refresh(const QPointF &pos)
{
	struct plot_data *entry;
	static QPixmap tissues(16,60);
	static QPainter painter(&tissues);
	static struct membuffer mb = { 0 };

	int time = timeAxis->valueAt(pos);
	if (time == lastTime)
		return;

	lastTime = time;
	clear();

	mb.len = 0;
	entry = get_plot_details_new(&pInfo, time, &mb);
	if (entry) {
		tissues.fill();
		painter.setPen(QColor(0, 0, 0, 0));
		painter.setBrush(QColor(LIMENADE1));
		painter.drawRect(0, 10 + (100 - AMB_PERCENTAGE) / 2, 16, AMB_PERCENTAGE / 2);
		painter.setBrush(QColor(SPRINGWOOD1));
		painter.drawRect(0, 10, 16, (100 - AMB_PERCENTAGE) / 2);
		painter.setBrush(QColor(Qt::red));
		painter.drawRect(0,0,16,10);
		painter.setPen(QColor(0, 0, 0, 255));
		painter.drawLine(0, 60 - entry->gfline / 2, 16, 60 - entry->gfline / 2);
		painter.drawLine(0, 60 - AMB_PERCENTAGE * (entry->pressures.n2 + entry->pressures.he) / entry->ambpressure / 2,
				16, 60 - AMB_PERCENTAGE * (entry->pressures.n2 + entry->pressures.he) / entry->ambpressure /2);
		painter.setPen(QColor(0, 0, 0, 127));
		for (int i=0; i<16; i++) {
			painter.drawLine(i, 60, i, 60 - entry->percentages[i] / 2);
		}
		entryToolTip.first->setPixmap(tissues);
		entryToolTip.second->setText(QString::fromUtf8(mb.buffer, mb.len));
	}

	Q_FOREACH (QGraphicsItem *item, scene()->items(pos, Qt::IntersectsItemBoundingRect
		,Qt::DescendingOrder, scene()->views().first()->transform())) {
		if (!item->toolTip().isEmpty())
			addToolTip(item->toolTip());
	}
	expand();
}

void ToolTipItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	oldSelection = scene()->selectedItems();
	scene()->clearSelection();
	QGraphicsItem::mousePressEvent(event);
}
