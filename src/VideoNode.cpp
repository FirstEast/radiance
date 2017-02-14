#include "VideoNode.h"
#include "RenderContext.h"
#include <QDebug>
#include <QThread>

VideoNode::VideoNode(RenderContext *context)
    : m_context(context),
    m_fbos(context->outputCount()),
    m_displayFbos(context->outputCount()),
    m_renderFbos(context->outputCount()),
    m_updated(context->outputCount()),
    m_textureLocks(context->outputCount()),
    m_initialized(false)
{
    moveToThread(context->thread());
    emit m_context->addVideoNodeRequested(this);
    for(int i=0; i<m_context->outputCount(); i++) m_textureLocks[i] = new QMutex();
//    m_context->addVideoNode(this);
}

void VideoNode::render() {
    if(!m_initialized) {
        initializeOpenGLFunctions(); // Placement of this function is black magic to me
        initialize();
        for(int i=0; i<m_context->outputCount(); i++) m_updated[i] = false;
        m_initialized = true;
        emit initialized();
    }
    paint();
}

// This function is called from paint()
// to draw onto the back-buffer.
void VideoNode::blitToRenderFbo() {
    m_context->flush(); // Flush before taking the lock to speed things up a bit

    glClearColor(0, 0, 0, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    for(int i=0; i<m_context->outputCount(); i++) {
        QMutexLocker locker(m_textureLocks[i]);
        auto size = m_fbos.at(i)->size();
        resizeFbo(&m_renderFbos[i], size);

        glViewport(0, 0, size.width(), size.height());

        float values[] = {
            -1, -1,
            1, -1,
            -1, 1,
            1, 1
        };

        m_renderFbos.at(i)->bind();
        m_context->m_premultiply->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_fbos.at(i)->texture());
        m_context->m_premultiply->setAttributeArray(0, GL_FLOAT, values, 2);
        m_context->m_premultiply->setUniformValue("iFrame", 0);
        m_context->m_premultiply->enableAttributeArray(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_context->m_premultiply->disableAttributeArray(0);
        m_context->m_premultiply->release();

        m_context->flush();
        m_updated[i] = true;
    }
    QOpenGLFramebufferObject::bindDefault();

    //QOpenGLFramebufferObject::blitFramebuffer(m_renderPreviewFbo, m_previewFbo);

    //QMutexLocker imageLocker(&m_previewImageLock);
    //m_previewImageValid = false;
}

// This function gets the pixel values at the given points
QVector<QColor> VideoNode::pixels(int i, QVector<QPointF> points) {
    QMutexLocker imageLocker(&m_previewImageLock);
    if (!m_previewImageValid) {
        QMutexLocker locker(m_textureLocks.at(i));
        m_previewImage = m_fbos.at(i)->toImage();
        m_previewImageValid = true;
    }
    QVector<QColor> output;
    for (QPointF point : points) {
        QPointF scaled_point = point;
        scaled_point.rx() *= m_previewImage.width();
        scaled_point.ry() *= m_previewImage.height();
        output.append(m_previewImage.pixel(scaled_point.toPoint()));
    }
    return output;
}

// This function is called from the rendering thread
// to get the latest frame in m_displayFbo[i].
// It returns true if there is a new frame
bool VideoNode::swap(int i) {
    QMutexLocker locker(m_textureLocks[i]);
    if(m_updated.at(i)) {
        m_context->flush();
        qSwap(m_displayFbos[i], m_renderFbos[i]);
        //resizeFbo(&m_displayFbos[i], m_renderFbos.at(i)->size());
        //QOpenGLFramebufferObject::blitFramebuffer(m_displayFbos.at(i), m_renderFbos.at(i));
        m_updated[i] = false;
        return true;
    }
    return false;
}

// Before deleting any FBOs and whatnot, we need to
// 1. Make sure we aren't currently rendering
// 2. Remove ourselves from the context graph
void VideoNode::beforeDestruction() {
    m_context->removeVideoNode(this);
}

VideoNode::~VideoNode() {
    for(int i=0; i<m_context->outputCount(); i++) {
        delete m_fbos.at(i);
        m_fbos[i] = 0;
        delete m_renderFbos.at(i);
        m_renderFbos[i] = 0;
        delete m_displayFbos.at(i);
        m_displayFbos[i] = 0;
        delete m_textureLocks.at(i);
        m_textureLocks[i] = 0;
    }
    // Stop event processing, move the thread to GUI and make sure it is deleted.
    //moveToThread(QGuiApplication::instance()->thread());
}

void VideoNode::resizeFbo(QOpenGLFramebufferObject **fbo, QSize size) {
    if((*fbo == NULL) || (*fbo)->size() != size) {
        delete *fbo;
        *fbo = new QOpenGLFramebufferObject(size);
        glBindTexture(GL_TEXTURE_2D, (*fbo)->texture());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

QSet<VideoNode*> VideoNode::dependencies() {
    return QSet<VideoNode*>();
}


RenderContext *VideoNode::context() {
    return m_context;
}
