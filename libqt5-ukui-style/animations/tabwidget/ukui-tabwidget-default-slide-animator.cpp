/*
 * Qt5-UKUI's Library
 *
 * Copyright (C) 2020, Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: Yue Lan <lanyue@kylinos.cn>
 *
 */

#include "ukui-tabwidget-default-slide-animator.h"

#include <QTabWidget>
#include <QStackedWidget>

#include <QEvent>
#include <QChildEvent>

#include <QPainter>

#include <QTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QTabBar>

#include <QDebug>

using namespace UKUI::TabWidget;

/*!
 * \brief DefaultSlideAnimator::DefaultSlideAnimator
 * \param parent
 * \details
 * This class define a slide animator for tab widget sliding animation.
 * Animator based on QVariantAnimation, paint on a tmp widgets when running.
 * The content of widget is based on animation's current value and 2 pixmap
 * grabbed at appropriate times.
 *
 * \note
 * Once an animator have bound a tab widget, it have to unbound current widget at first.
 * Then it can bind another tab widget again.
 */
DefaultSlideAnimator::DefaultSlideAnimator(QObject *parent) : QVariantAnimation (parent)
{
    setDuration(200);
    setEasingCurve(QEasingCurve::OutQuad);
    setStartValue(0.0);
    setEndValue(1.0);
}

/*!
 * \brief DefaultSlideAnimator::bindTabWidget
 * \param w A QTabWidget instance, most passed in QStyle::polish().
 * \return result if Tab widget be bound \c true for binding successed.
 * \details
 * When do a tab widget binding, animator will create a tmp child page for tab widget's
 * stack widget. Then it will watched their event waiting for preparing and doing a animation.
 */
bool DefaultSlideAnimator::bindTabWidget(QTabWidget *w)
{
    if (w) {
        m_bound_widget = w;

        //watch tab widget
        w->installEventFilter(this);

        m_tmp_page = new QWidget;
        //watch tmp page;
        m_tmp_page->installEventFilter(this);

        for (auto child : w->children()) {
            if (child->objectName() == "qt_tabwidget_stackedwidget") {
                auto stack = qobject_cast<QStackedWidget *>(child);
                m_stack = stack;
                //watch stack widget
                m_tmp_page->setParent(m_stack);
                m_stack->installEventFilter(this);
                break;
            }
        }

        for (int i = 0; i < w->count(); i++) {
            //watch sub page
            watchSubPage(w->widget(i));
        }

        previous_widget = m_bound_widget->currentWidget();

        connect(w, &QTabWidget::currentChanged, this, [=](int index){
                    this->stop();
                    m_tmp_page->hide();
                    if (m_bound_widget->currentWidget() && m_bound_widget->currentWidget() != previous_widget) {
                        left_right = m_bound_widget->currentIndex() > pervIndex;
                        pervIndex = m_bound_widget->currentIndex();
                        /*
                         * This way not suitable for 4k
                        */
                        //m_next_pixmap = m_bound_widget->grab(QRect(m_bound_widget->rect().x(), m_bound_widget->tabBar()->height(),
                        //m_bound_widget->currentWidget()->width(), m_bound_widget->currentWidget()->height()));

                        QPixmap pixmap(m_bound_widget->currentWidget()->size());

                        /*
                         * This way some widget such as QFrame.
                         * QPalette::Window was used to draw the background during the screenshot,
                         * but QWidget itself did not draw the entire area with others during the screenshot,
                         * resulting in some areas in the screenshot that the background drawn by QPalette::Window was different from the actual drawing.
                        */
                        //m_bound_widget->currentWidget()->render(&pixmap, QPoint(), m_bound_widget->currentWidget()->rect());

                        /*
                         * This way by modifying the way of QPalette, get the same screenshot as the actual state
                        */
                        //QPalette palette, palette_save;
                        //palette = palette_save = m_bound_widget->currentWidget()->palette();
                        //palette.setBrush(QPalette::Window, palette.brush(QPalette::Base));
                        //m_bound_widget->currentWidget()->setPalette(palette);
                        //m_bound_widget->currentWidget()->render(&pixmap, QPoint(), m_bound_widget->currentWidget()->rect());
                        //m_bound_widget->currentWidget()->setPalette(palette_save);

                        m_bound_widget->render(&pixmap, QPoint(), QRect(m_bound_widget->currentWidget()->rect().left() + 2,
                                                                        m_bound_widget->currentWidget()->rect().top() + m_bound_widget->tabBar()->height(),
                                                                        m_bound_widget->currentWidget()->width(), m_bound_widget->currentWidget()->height()));
                        m_next_pixmap = pixmap;

                        if (qobject_cast<QWidget *>(previous_widget)) {
                            QPixmap previous_pixmap(previous_widget->size());
                            QPalette palette, palette_save;
                            palette = palette_save = previous_widget->palette();
                            /*
                             * This use QPalette::Base to replace QPalette::Window, Mabey have unknow bug.
                            */
                            palette.setBrush(QPalette::Window, palette.brush(QPalette::Base));
                            previous_widget->setPalette(palette);
                            previous_widget->render(&previous_pixmap);
                            previous_widget->setPalette(palette_save);
                            m_previous_pixmap = previous_pixmap;
                            this->start();
                            m_tmp_page->raise();
                            m_tmp_page->show();
                        }
                    }
                    previous_widget = m_bound_widget->currentWidget();
                });

        connect(this, &QVariantAnimation::valueChanged, m_tmp_page, [=]() {
            m_tmp_page->repaint();
        });
        connect(this, &QVariantAnimation::finished, m_tmp_page, [=]() {
            m_tmp_page->repaint();
        });

        return true;
    }
    return false;
}

bool DefaultSlideAnimator::unboundTabWidget()
{
    clearPixmap();
    if (m_bound_widget) {
        disconnect(m_bound_widget, &QTabWidget::currentChanged, this, nullptr);
        for (auto w : m_bound_widget->children()) {
            w->removeEventFilter(this);
        }

        m_tmp_page->removeEventFilter(this);
        m_tmp_page->deleteLater();
        m_tmp_page = nullptr;
        previous_widget = nullptr;
        m_bound_widget = nullptr;
        this->deleteLater();
        return true;
    }
    return false;
}

bool DefaultSlideAnimator::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_tmp_page) {
        return filterTmpPage(obj, e);
    }
    if (obj == m_stack) {
        return filterStackedWidget(obj, e);
    }
    if (obj == m_bound_widget) {
        return filterTabWidget(obj, e);
    }
    return filterSubPage(obj, e);
}

void DefaultSlideAnimator::watchSubPage(QWidget *w)
{
    if (w)
        w->installEventFilter(this);
}

bool DefaultSlideAnimator::filterTabWidget(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::Close) {
        this->unboundTabWidget();
    }
    return false;
}

bool DefaultSlideAnimator::filterStackedWidget(QObject *obj, QEvent *e)
{
    switch (e->type()) {
    case QEvent::ChildAdded:
    case QEvent::ChildRemoved: {
        qDebug()<<"added/removed"<<obj;
        if (obj->objectName() == "qt_tabwidget_stackedwidget") {
            QChildEvent *ce = static_cast<QChildEvent *>(e);
            if (!ce->child()->isWidgetType())
                return false;
            if (ce->added()) {
                ce->child()->installEventFilter(this);
            } else {
                ce->child()->removeEventFilter(this);
            }
        }
        return false;
    }
    case QEvent::Resize:
        //qDebug()<<"resize";
        m_tab_resizing = true;
        return false;
    case QEvent::LayoutRequest: {
        /// there a 2 case we need excute these codes.
        /// 1. when stacked widget created and shown, it first do resize, then do a layout request.
        /// 2. after stacked widget resize.
        ///
        /// This event is very suitable for the above two situations,
        /// both in terms of efficiency and trigger time.
        if (m_tab_resizing) {
            m_tmp_page->resize(m_stack->size());
            if(m_next_pixmap.isNull())
                pervIndex = m_bound_widget->currentIndex();
        }
        m_tab_resizing = false;
        return false;
    }
    default:
        break;
    }
    return false;
}

bool DefaultSlideAnimator::filterSubPage(QObject *obj, QEvent *e)
{
    switch (e->type()) {
    case QEvent::Show: {
        return false;
    }
    case QEvent::Hide: {
        /*
         * This way not suitable for 4k and multi-screen crash(Todo: get the screen change event, get the corresponding screen,
         * call the grabWindow method of QScreen to get the picture,
         * but the picture needs to be processed by 4k and orientation size)
        */
        //if (m_bound_widget->currentWidget()) {
        //QScreen *screen = QGuiApplication::primaryScreen();
        //m_previous_pixmap = screen->grabWindow(m_bound_widget->winId(),
        //                                       m_bound_widget->tabBar()->x() + 2,
        //                                       m_bound_widget->tabBar()->height(),
        //                                       m_bound_widget->currentWidget()->width(),
        //                                       m_bound_widget->currentWidget()->height());
        //}
        this->stop();
        return false;
    }
    case QEvent::Resize: {
        this->stop();
        return false;
    }
    default:
        return false;
    }
}

bool DefaultSlideAnimator::filterTmpPage(QObject *obj, QEvent *e)
{
    switch (e->type()) {
    case QEvent::Show: {
        return false;
    }
    case QEvent::Paint: {
        QWidget *w = qobject_cast<QWidget *>(obj);
        if (this->state() == QAbstractAnimation::Running) {
            QPainter p(w);
            auto value = this->currentValue().toDouble();
            p.setRenderHints(QPainter::Antialiasing);

            //do a horizon slide.
            auto prevSrcRect = QRectF(m_previous_pixmap.rect());
            auto prevTargetRect = QRectF(m_previous_pixmap.rect());
            auto nextSrcRect = QRectF(m_next_pixmap.rect());
            auto nextTargetRect = QRectF(m_next_pixmap.rect());
            if (left_right) {
                prevSrcRect.setX(m_previous_pixmap.width() * value);
                prevSrcRect.setWidth(m_previous_pixmap.width() * (1 - value));
                prevTargetRect.setWidth(m_previous_pixmap.width() * (1 - value));
                p.drawPixmap(prevTargetRect, m_previous_pixmap, prevSrcRect);

                nextSrcRect.setWidth(m_next_pixmap.width() * value);
                nextTargetRect.setX(m_next_pixmap.width() * (1 - value));
                nextTargetRect.setWidth(m_next_pixmap.width() * value);
                p.drawPixmap(nextTargetRect, m_next_pixmap, nextSrcRect);
            }
            else {
                nextSrcRect.setX(m_next_pixmap.width() * (1 - value));
                nextSrcRect.setWidth(m_next_pixmap.width() * value);
                nextTargetRect.setWidth(m_next_pixmap.width() * value);
                p.drawPixmap(nextTargetRect, m_next_pixmap, nextSrcRect);

                prevSrcRect.setWidth(m_previous_pixmap.width() * (1 - value));
                prevTargetRect.setX(m_previous_pixmap.width() * value);
                prevTargetRect.setWidth(m_previous_pixmap.width() * (1 - value));
                p.drawPixmap(prevTargetRect, m_previous_pixmap, prevSrcRect);
            }

            //eat event so that widget will not paint default items and override
            //our custom pixmap.
            return true;
        }
        m_tmp_page->hide();
        if (!m_next_pixmap.isNull())
            m_stack->stackUnder(m_tmp_page);
        return false;
    }
    default:
        return false;
    }
}

void DefaultSlideAnimator::clearPixmap()
{
    m_previous_pixmap = QPixmap();
    m_next_pixmap = QPixmap();
}
