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

#include "audiowaveformscopewidget.h"
#include <QDebug>
#include <QPainter>
#include <QResizeEvent>

static const qreal MAX_AMPLITUDE = 32768.0;

static int graphHeight(const QSize& widgetSize, int maxChan, int padding)
{
    int totalPadding = padding + (padding * maxChan);
    return (widgetSize.height() - totalPadding) / maxChan;
}

static int graphBottomY(const QSize& widgetSize, int channel, int maxChan, int padding)
{
    int gHeight = graphHeight(widgetSize, maxChan, padding);
    return padding + (gHeight + padding) * channel;
}

static int graphTopY(const QSize& widgetSize, int channel, int maxChan, int padding)
{
    int gHeight = graphHeight(widgetSize, maxChan, padding);
    return graphBottomY(widgetSize, channel, maxChan, padding) + gHeight;
}

static int graphCenterY(const QSize& widgetSize, int channel, int maxChan, int padding)
{
    int gHeight = graphHeight(widgetSize, maxChan, padding);
    return graphBottomY(widgetSize, channel, maxChan, padding) + gHeight / 2;
}

AudioWaveformScopeWidget::AudioWaveformScopeWidget()
  : ScopeWidget("AudioWaveform")
  , m_frame()
  , m_prevSize(0, 0)
  , m_renderWave()
  , m_refreshTime()
  , m_graphTopPadding(0)
  , m_channels(0)
  , m_mutex(QMutex::NonRecursive)
  , m_displayWave()
  , m_displayGrid()
  , m_size(0, 0)
{
    qDebug() << "begin";
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
    setMinimumSize(100, 100);
    m_refreshTime.start();
    qDebug() << "end";
}

AudioWaveformScopeWidget::~AudioWaveformScopeWidget()
{
    qDebug() << "begin";
    qDebug() << "end";
}

void AudioWaveformScopeWidget::refreshScope()
{
    m_mutex.lock();
    QSize currentSize = m_size;
    m_mutex.unlock();

    while (m_queue.count() > 0) {
        m_frame = m_queue.pop();
    }
    
    int channels = m_frame.get_audio_channels();
    channels = channels ? channels : 2;

    if (m_prevSize == currentSize && m_refreshTime.elapsed() < 90) {
        // When not resizing, limit refreshes to 90ms.
        return;
    }

    if (m_prevSize != currentSize || channels != m_channels) {
        m_channels = channels;
        createGrid(currentSize);
    }

    if (m_renderWave.size() != currentSize) {
        m_renderWave = QImage(currentSize, QImage::Format_ARGB32_Premultiplied);
    }

    m_renderWave.fill(Qt::transparent);

    QPainter p(&m_renderWave);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(palette().dark().color().rgb());

    if (m_frame.is_valid() && m_frame.get_audio_samples() > 0) {

        int samples = m_frame.get_audio_samples();
        int16_t* audio = (int16_t*)m_frame.get_audio();
        int waveAmplitude = graphHeight(currentSize, m_channels, m_graphTopPadding) / 2;
        qreal scaleFactor = (qreal)waveAmplitude / (qreal)MAX_AMPLITUDE;

        for (int c = 0; c < m_channels; c++)
        {
            p.save();
            int y = graphCenterY(currentSize, c, m_channels, m_graphTopPadding);
            p.translate(0, y);

            // For each x position on the waveform, find the min and max sample
            // values that apply to that position. Draw a vertical line from the
            // min value to the max value.
            QPoint high;
            QPoint low;
            int lastX = 0;
            const int16_t* q = audio + c;
            qreal max = *q;
            qreal min = *q;

            for (int i = 0; i <= samples; i++)
            {
                int x = ( i * currentSize.width() ) / samples;
                if (x != lastX) {
                    // The min and max have been determined for the previous x
                    // So draw the line
                    high.setX(lastX);
                    high.setY(max * scaleFactor);
                    low.setX(lastX);
                    low.setY(min * scaleFactor);
                    if (high.y() == low.y()) {
                        p.drawPoint(high);
                    } else {
                        p.drawLine(low, high);
                    }
                    lastX = x;

                    // Swap max and min so that the next line picks up where
                    // this one left off.
                    int tmp = max;
                    max = min;
                    min = tmp;
                }

                if (*q > max) max = *q;
                if (*q < min) min = *q;
                q += channels;
            }
            p.restore();
        }
    }

    p.end();

    m_mutex.lock();
    m_displayWave.swap(m_renderWave);
    m_mutex.unlock();

    m_prevSize = currentSize;
    m_refreshTime.restart();
}

void AudioWaveformScopeWidget::createGrid(const QSize& size)
{
    QFont font = QWidget::font();
    int fontSize = font.pointSize() - (font.pointSize() > 10? 2 : (font.pointSize() > 8? 1 : 0));
    font.setPointSize(fontSize);
    QFontMetrics fm(font);
    QString zeroLabel = tr("0");
    QString infinityLabel = tr("-inf");
    QRect textRect = fm.tightBoundingRect( infinityLabel );
    int labelHeight = textRect.height();
    m_graphTopPadding = fm.height();
    m_graphLeftPadding = textRect.width() + 6;

    m_mutex.lock();

    m_displayGrid = QImage(size, QImage::Format_ARGB32_Premultiplied);
    m_displayGrid.fill(Qt::transparent);
    QPainter p(&m_displayGrid);
    QColor penColor(palette().text().color());
    penColor.setAlpha(255/2);
    QPen pen(penColor);
    pen.setWidth(0);
    p.setPen(pen);
    p.setFont(font);

    for (int c = 0; c < m_channels; c++) {
        QPoint textLoc(0, 0);
        QPoint lineBegin(m_graphLeftPadding, 0);
        QPoint lineEnd(size.width() - 1, 0);
        int y = 0;

        y = graphBottomY(size, c, m_channels, m_graphTopPadding);
        textLoc.setY(y + labelHeight / 2);
        textLoc.setX( (m_graphLeftPadding - fm.width(zeroLabel)) / 2);
        p.drawText( textLoc, zeroLabel );
        lineBegin.setY(y);
        lineEnd.setY(y);
        p.drawLine(lineBegin, lineEnd);

        y = graphCenterY(size, c, m_channels, m_graphTopPadding);
        textLoc.setY(y + labelHeight / 2);
        textLoc.setX( (m_graphLeftPadding - fm.width(infinityLabel)) / 2);
        p.drawText( textLoc, infinityLabel );
        lineBegin.setY(y);
        lineEnd.setY(y);
        p.drawLine(lineBegin, lineEnd);

        y = graphTopY(size, c, m_channels, m_graphTopPadding);
        textLoc.setY(y + labelHeight / 2);
        textLoc.setX( (m_graphLeftPadding - fm.width(zeroLabel)) / 2);
        p.drawText( textLoc, zeroLabel );
        lineBegin.setY(y);
        lineEnd.setY(y);
        p.drawLine(lineBegin, lineEnd);
    }

    p.end();

    m_mutex.unlock();
}

void AudioWaveformScopeWidget::paintEvent(QPaintEvent*)
{
    if (!isVisible())
        return;

    QPainter p(this);
    m_mutex.lock();
    p.drawImage(rect(), m_displayWave, m_displayWave.rect());
    p.drawImage(rect(), m_displayGrid, m_displayGrid.rect());
    m_mutex.unlock();
    p.end();
}

void AudioWaveformScopeWidget::resizeEvent(QResizeEvent*)
{
    m_mutex.lock();
    m_size = size();
    m_mutex.unlock();
    requestRefresh();
}

QString AudioWaveformScopeWidget::getTitle()
{
   return tr("Audio Waveform");
}
