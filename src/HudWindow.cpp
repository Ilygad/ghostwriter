/***********************************************************************
 *
 * Copyright (C) 2014-2016 wereturtle
 * Copyright(c) 2009 by Gabriel M. Beddingfield <gabriel@teuton.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***********************************************************************/

#include <QPainter>
#include <QBitmap>
#include <QBrush>
#include <QPen>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QStatusBar>
#include <QSizeGrip>
#include <QGraphicsEffect>
#include <QGraphicsColorizeEffect>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPoint>
#include <QTextStream>
#include <QtDebug>

#include "HudWindow.h"
#include "ColorHelper.h"

QT_BEGIN_NAMESPACE
extern void qt_blurImage
(
    QPainter *p,
    QImage& blurImage,
    qreal radius,
    bool quality,
    bool alphaOnly,
    int transposed = 0
);
QT_END_NAMESPACE


HudWindow::HudWindow(QWidget *parent)
    : QWidget(parent)
{
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    this->setMouseTracking(true);
    desktopCompositingEnabled = true;

    // Set up the close button.
    closeButton = new QPushButton(this);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setStyleSheet("margin: 1; padding: 0; border: 0; width: 16px; "
        "height: 16px; background: transparent; "
        "image: url(:/resources/images/close-delete.svg)");
    closeButtonColorEffect = new QGraphicsColorizeEffect();
    closeButtonColorEffect->setColor(QColor(Qt::white));
    closeButtonColorEffect->setStrength(1.0);
    closeButton->setGraphicsEffect(closeButtonColorEffect);
    closeButton->installEventFilter(this);
    closeButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

    // Set up initial window title (blank).
    windowTitleLabel = new QLabel("");
    windowTitleLabel->setObjectName("windowTitle");
    windowTitleLabel->setStyleSheet("border: 0; font-size: 10pt");
    windowTitleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Set up the size grip.
    QWidget* sizeGripContainer = new QWidget();
    QGridLayout* sizeGripLayout = new QGridLayout();
    sizeGripContainer->setLayout(sizeGripLayout);
    sizeGrip = new QSizeGrip(this);
    sizeGrip->setStyleSheet("width: 16px; height 16px; "
        "image: url(:/resources/images/size-grip.svg)");
    sizeGripColorEffect = new QGraphicsColorizeEffect();
    sizeGripColorEffect->setColor(QColor(Qt::white));
    sizeGripColorEffect->setStrength(1.0);
    sizeGrip->setGraphicsEffect(sizeGripColorEffect);
    sizeGrip->setVisible(true);
    sizeGripLayout->addWidget(sizeGrip, 0, 0, 1, 1);
    sizeGripLayout->setMargin(3);

    // Set up the layout for this window.
    QWidget* titleBar = new QWidget();
    titleBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    titleBarLayout = new QGridLayout();
    titleBar->setLayout(titleBarLayout);
    titleBar->setStyleSheet("margin: 0; border: 0; padding: 0");
    titleBarLayout->setMargin(1);
    titleBarLayout->addWidget
    (
        windowTitleLabel,
        0,
        1,
        1,
        2,
        Qt::AlignCenter | Qt::AlignTop
    );


    // Put the close button on the left if the OS is Mac OS X so that it
    // meshes with the window manager.  Otherwise, keep it on the right-hand
    // side.  Note that at the moment it seems detecting the window manager's
    // settings for which sides the window frame's buttons are displayed
    // is not possible.  Thus, defaulting to the right-hand side for all other
    // systems other than OS X seems to be the safest course of action for now.
    //
#ifdef Q_OS_MAC
    this->setButtonLayout(HudWindowButtonLayoutLeft);
#else
    this->setButtonLayout(HudWindowButtonLayoutRight);
#endif

    layout = new QGridLayout();
    layout->setMargin(11);
    layout->addWidget(titleBar, 0, 0, 1, 1, Qt::AlignTop);
    layout->addWidget(sizeGripContainer, 2, 0, 1, 1, Qt::AlignRight);
    layout->setRowStretch(0, 0);
    layout->setRowStretch(1, 1);
    layout->setRowStretch(2, 0);
    this->setLayout(layout);

    setForegroundColor(QColor(Qt::white));
    setBackgroundColor(QColor(0, 0, 0, 200));
    isTitleBarBeingDragged = false;
}

HudWindow::~HudWindow()
{

}

void HudWindow::setCentralWidget(QWidget* widget)
{
    layout->addWidget(widget, 1, 0, 1, 1);
    widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

void HudWindow::setWindowTitle(const QString& title)
{
    windowTitleLabel->setText(title);
}

QColor HudWindow::getForegroundColor()
{
    return foregroundColor;
}

void HudWindow::setForegroundColor(const QColor& color)
{
    foregroundColor = color;

    QString styleSheet = "";
    QTextStream stream(&styleSheet);

    stream <<
        "QLabel { color: rgb("
        << foregroundColor.red()
        << ", "
        << foregroundColor.green()
        << ", "
        << foregroundColor.blue()
        << "); background: transparent } #windowTitle { font-weight: bold } ";

    this->setStyleSheet(styleSheet);
    closeButtonColorEffect->setColor(foregroundColor);
    sizeGripColorEffect->setColor(foregroundColor);

    resetTitleButtonHoverColor();
}

QColor HudWindow::getBackgroundColor()
{
    return backgroundColor;
}

void HudWindow::setBackgroundColor(const QColor& color)
{
    backgroundColor = color;
    resetTitleButtonHoverColor();
}

void HudWindow::setSizeGripEnabled(bool enabled)
{
    sizeGrip->setVisible(enabled);
}

void HudWindow::setButtonLayout(HudWindowButtonLayout layout)
{
    static bool initialSetup = true;

    if (initialSetup)
    {
        initialSetup = false;
    }
    else
    {
        titleBarLayout->removeWidget(closeButton);
    }

    if (HudWindowButtonLayoutLeft == layout)
    {
        titleBarLayout->addWidget
        (
            closeButton,
            0,
            1,
            1,
            1,
            Qt::AlignLeft | Qt::AlignTop
        );
    }
    else
    {
        titleBarLayout->addWidget
        (
            closeButton,
            0,
            2,
            1,
            1,
            Qt::AlignRight | Qt::AlignTop
        );
    }
}

void HudWindow::setDesktopCompositingEnabled(bool enabled)
{
    desktopCompositingEnabled = enabled;

    if (enabled)
    {
        layout->setMargin(11);
        resizeEvent(NULL);
    }
    else
    {
        layout->setMargin(1);
    }

    repaint(rect());
}

QSize HudWindow::sizeHint() const
{
    return QSize(250, 400);
}

void HudWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (desktopCompositingEnabled)
    {
        // Draw the window shadow first.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawImage(0, 0, dropShadowImg);

        // And now draw the window itself.
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setPen(QPen(QBrush(foregroundColor), 0.5));
        painter.setBrush(QBrush(QBrush(backgroundColor)));
        painter.drawRoundedRect(rect().adjusted(10, 10, -10, -10), 5, 5);
        painter.end();
    }
    else
    {
        // Just draw a rectangular window with square corners if desktop
        // compositing is disabled.  Also, make sure the backgroundColor
        // isn't transparent (set alpha to 255), otherwise the background
        // color of the window will be strange.
        //
        QColor bgColor = backgroundColor;
        bgColor.setAlpha(255);

        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setPen(QPen(QBrush(foregroundColor), 0.5));
        painter.setBrush(QBrush(QBrush(bgColor)));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
        painter.end();
    }
}

void HudWindow::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);

    if (desktopCompositingEnabled)
    {
        // Pre-draw the window drop shadow.  It only needs to be drawn upon
        // resize.  We do this because applying a blur effect to the drop
        // shadow is computationally expensive.

        // First, draw the shadow, using a pleasant gradient.
        QLinearGradient shadowGradient;
        shadowGradient.setStart(rect().width() / 2, 0.0);
        shadowGradient.setFinalStop(rect().width() / 2, rect().height());
        shadowGradient.setColorAt(0.0, QColor(0, 0, 0, 20));
        shadowGradient.setColorAt(1.0, QColor(0, 0, 0, 200));

        QImage unblurredImage
            (
                rect().width(),
                rect().height(),
                QImage::Format_ARGB32_Premultiplied
            );
        unblurredImage.fill(Qt::transparent);

        QPainter painter(&unblurredImage);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::NoPen));
        painter.setBrush(QBrush(shadowGradient));
        painter.drawRoundedRect(rect().adjusted(10, 10, -10, -8), 5, 5);
        painter.end();

        // Now we need to blur the shadow onto its final destination image,
        // dropShadowImg, which will be drawn on the next paintEvent().
        //
        dropShadowImg = QImage
            (
                rect().width(),
                rect().height(),
                QImage::Format_ARGB32_Premultiplied
            );
        dropShadowImg.fill(Qt::transparent);

        painter.begin(&dropShadowImg);
        painter.setRenderHint(QPainter::Antialiasing);

        // Note that the blur only applies to the alpha channel.
        qt_blurImage(&painter, unblurredImage, 20, true, true);
        painter.end();
    }
}

void HudWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() & Qt::LeftButton)
    {
        isTitleBarBeingDragged = true;
        mouseDragStartingPoint = event->globalPos() - frameGeometry().topLeft();
        this->setCursor(Qt::SizeAllCursor);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void HudWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (isTitleBarBeingDragged)
    {
        isTitleBarBeingDragged = false;
        setCursor(Qt::ArrowCursor);
    }

    // Ignore the event in case the parent widget wants to change the
    // cursor icon to something else.
    //
    event->ignore();
}

void HudWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (isTitleBarBeingDragged)
    {
        move(event->globalPos() - mouseDragStartingPoint);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

bool HudWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == closeButton)
    {
        if (QEvent::HoverEnter == event->type())
        {
            closeButtonColorEffect->setColor(titleBarButtonHoverColor);
        }
        else if (QEvent::HoverLeave == event->type())
        {
            closeButtonColorEffect->setColor(foregroundColor);
        }
    }

    return false;
}

void HudWindow::resetTitleButtonHoverColor()
{
    // Set up the close button hover color based on the foreground
    // and background colors.
    titleBarButtonHoverColor = ColorHelper::applyAlpha
        (
            foregroundColor,
            backgroundColor,
            200
        );
}
