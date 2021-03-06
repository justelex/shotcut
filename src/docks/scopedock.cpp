/*
 * Copyright (c) 2015 Meltytech, LLC
 * Author: Brian Matherly <code@brianmatherly.com>
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
 */

#include "scopedock.h"
#include "controllers/scopecontroller.h"
#include <QDebug>
#include <QtWidgets/QScrollArea>
#include <QAction>

ScopeDock::ScopeDock(ScopeController* scopeController, ScopeWidget* scopeWidget) :
    QDockWidget()
  , m_scopeController(scopeController)
  , m_scopeWidget(scopeWidget)
{
    qDebug() << "begin";
    setObjectName(m_scopeWidget->objectName() + "Dock");
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(m_scopeWidget);
    QDockWidget::setWidget(scrollArea);
    QDockWidget::setWindowTitle(m_scopeWidget->getTitle());

    connect(toggleViewAction(), SIGNAL(toggled(bool)), this, SLOT(onActionToggled(bool)));
    qDebug() << "end";
}

void ScopeDock::onActionToggled(bool checked)
{
    if(checked) {
        connect(m_scopeController, SIGNAL(newFrame(const SharedFrame&)), m_scopeWidget, SLOT(onNewFrame(const SharedFrame&)));
    } else {
        disconnect(m_scopeController, SIGNAL(newFrame(const SharedFrame&)), m_scopeWidget, SLOT(onNewFrame(const SharedFrame&)));
    }
}
