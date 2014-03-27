#include "opengl_renderer.h"

#include "ui/graphics/opengl/gl_shortcuts.h"



QnOpenGLRenderer::QnOpenGLRenderer(const QGLContext* a_context , QObject *parent)
{
    m_colorProgram.reset(new QnColorGLShaderProgramm(a_context,parent));
    m_colorProgram->compile();
    m_textureColorProgram.reset(new QnTextureColorGLShaderProgramm(a_context,parent));
    m_textureColorProgram->compile();

    m_texturePerVertexColoredProgram.reset(new QnPerVertexColoredGLShaderProgramm(a_context,parent));
    m_texturePerVertexColoredProgram->compile();

    

    m_indices_for_render_quads[0] = 0;
    m_indices_for_render_quads[1] = 1;
    m_indices_for_render_quads[2] = 2;

    m_indices_for_render_quads[3] = 0;
    m_indices_for_render_quads[4] = 2;
    m_indices_for_render_quads[5] = 3;

    m_color = QVector4D(1.0f,1.0f,1.0f,1.0f);
    m_modelViewMatrix.setToIdentity();
    m_projectionMatrix.setToIdentity();
}
void  QnOpenGLRenderer::setColor(const QVector4D& c)
{ 
    m_color = c; 
};
void  QnOpenGLRenderer::setColor(const QColor& c)
{ 
    m_color.setX(c.redF()); 
    m_color.setY(c.greenF()); 
    m_color.setZ(c.blueF()); 
    m_color.setW(c.alphaF()); 
};
void QnOpenGLRenderer::drawBindedTextureOnQuad( const QRectF &rect , QnTextureColorGLShaderProgramm* shader)
{
    GLfloat vertices[] = {
        (GLfloat)rect.left(),   (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.bottom(),
        (GLfloat)rect.left(),   (GLfloat)rect.bottom()
    };

    GLfloat texCoords[] = {
        0.0, 0.0,
        1.0, 0.0,
        1.0, 1.0,
        0.0, 1.0
    };
    drawBindedTextureOnQuad(vertices,texCoords,shader);
};
void QnOpenGLRenderer::drawBindedTextureOnQuad( const QRectF &rect , const QSizeF& size, QnTextureColorGLShaderProgramm* shader)
{
    GLfloat vertices[] = {
        (GLfloat)rect.left(),   (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.bottom(),
        (GLfloat)rect.left(),   (GLfloat)rect.bottom()
    };

    GLfloat texCoords[] = {
        0.0, 0.0,
        size.width(), 0.0,
        size.width(), size.height(),
        0.0, size.height()
    };
    drawBindedTextureOnQuad(vertices,texCoords,shader);
};

void QnOpenGLRenderer::drawColoredQuad( const QRectF &rect , QnColorGLShaderProgramm* shader )
{
    GLfloat vertices[] = {
        (GLfloat)rect.left(),   (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.bottom(),
        (GLfloat)rect.left(),   (GLfloat)rect.bottom()
    };
    drawColoredQuad(vertices,shader);
};
void QnOpenGLRenderer::drawPerVertexColoredPolygon( const std::vector<float>& a_positions , const std::vector<float>& a_colors)
{
    QnPerVertexColoredGLShaderProgramm* shader = m_texturePerVertexColoredProgram.data();

    if ( shader )
    {
        const int VERTEX_POS_SIZE = 2; // x, y
        const int VERTEX_COLOR_SIZE = 4; // s and t
        const int VERTEX_POS_INDX = 0;
        const int VERTEX_COLOR_INDX = 1;
        glCheckError("render");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(VERTEX_POS_INDX);
        glEnableVertexAttribArray(VERTEX_COLOR_INDX);

        glVertexAttribPointer(VERTEX_POS_INDX, VERTEX_POS_SIZE, GL_FLOAT, GL_FALSE, 0, &a_positions[0]);
        glVertexAttribPointer(VERTEX_COLOR_INDX,VERTEX_COLOR_SIZE, GL_FLOAT,GL_FALSE, 0, &a_colors[0]);
        glCheckError("render");


        shader->bind();
        shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);
        shader->setColor(m_color);
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        shader->bindAttributeLocation("aColor",VERTEX_COLOR_INDX);
        
        glDrawArrays(GL_TRIANGLE_FAN,0,a_positions.size()/VERTEX_POS_SIZE);
        
        shader->release();
        glCheckError("render");
        glDisableVertexAttribArray(VERTEX_COLOR_INDX);
        glDisableVertexAttribArray(VERTEX_POS_INDX);
    }
}

void QnOpenGLRenderer::drawColoredQuad(const float* v_array, QnColorGLShaderProgramm* shader)
{
    if ( !shader )
        shader = m_colorProgram.data();
    if ( shader )
    {
        const int VERTEX_POS_SIZE = 2; // x, y
        const int VERTEX_POS_INDX = 0;
        glCheckError("render");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(VERTEX_POS_INDX);
        glVertexAttribPointer(VERTEX_POS_INDX, VERTEX_POS_SIZE, GL_FLOAT, GL_FALSE, 0, v_array);
        glCheckError("render");


        shader->bind();
        shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);
        shader->setColor(m_color);
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,m_indices_for_render_quads);
        glCheckError("render");
        shader->release();
        glDisableVertexAttribArray(VERTEX_POS_INDX);
    }
};
void QnOpenGLRenderer::drawBindedTextureOnQuad( const float* v_array, const float* tx_array , QnAbstractBaseGLShaderProgramm* shader)
{
    bool empty_shader = false;
    if ( !shader )
    {
        shader = m_textureColorProgram.data();
        empty_shader = true;
    };

    if ( shader )
    {
        const int VERTEX_POS_SIZE = 2; // x, y
        const int VERTEX_TEXCOORD0_SIZE = 2; // s and t
        const int VERTEX_POS_INDX = 0;
        const int VERTEX_TEXCOORD0_INDX = 1;
        glCheckError("render");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(VERTEX_POS_INDX);
        glEnableVertexAttribArray(VERTEX_TEXCOORD0_INDX);

        glVertexAttribPointer(VERTEX_POS_INDX, VERTEX_POS_SIZE, GL_FLOAT, GL_FALSE, 0, v_array);
        glVertexAttribPointer(VERTEX_TEXCOORD0_INDX,VERTEX_TEXCOORD0_SIZE, GL_FLOAT,GL_FALSE, 0, tx_array);
        glCheckError("render");
        if (empty_shader)
            m_textureColorProgram->bind();
        glCheckError("render");
        shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);
        glCheckError("render");
        if (empty_shader)
            m_textureColorProgram->setColor(m_color);
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        glCheckError("render");
        shader->bindAttributeLocation("aTexcoord",VERTEX_TEXCOORD0_INDX);
        glCheckError("render");
        if (empty_shader)
            m_textureColorProgram->setTexture(0);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,m_indices_for_render_quads);
        if (empty_shader)
            m_textureColorProgram->release();
        glCheckError("render");
        glDisableVertexAttribArray(VERTEX_TEXCOORD0_INDX);
        glDisableVertexAttribArray(VERTEX_POS_INDX);
    }
    
}
void    QnOpenGLRenderer:: drawColoredPolygon( const QPolygonF & a_polygon, QnColorGLShaderProgramm* shader)
{
    std::vector<float> vertices(a_polygon.size()*2);
    for ( int i = 0 ; i < a_polygon.size() ; i++)
    {
        vertices[i*2] = a_polygon[i].x();
        vertices[i*2+1] = a_polygon[i].y();
    }
    drawColoredPolygon(&vertices[0],a_polygon.size(),shader);
};
void    QnOpenGLRenderer:: drawColoredPolygon( const QVector<QVector2D>& a_polygon, QnColorGLShaderProgramm* shader)
{
    //qDebug()<<"ModelView"<<m_modelViewMatrix;
    //qDebug()<<"Projection"<<m_projectionMatrix;

    std::vector<float> vertices(a_polygon.size()*2);
    for ( int i = 0 ; i < a_polygon.size() ; i++)
    {
        vertices[i*2] = a_polygon[i].x();
        vertices[i*2+1] = a_polygon[i].y();
        //qDebug()<<vertices[i*2]<<vertices[i*2+1];
    }
    drawColoredPolygon(&vertices[0],a_polygon.size(),shader);
};
void    QnOpenGLRenderer:: drawColoredPolygon( const float* v_array, unsigned int size , QnColorGLShaderProgramm* shader)
{
    if ( !shader )
        shader = m_colorProgram.data();
    if ( shader )
    {
        const int VERTEX_POS_SIZE = 2; // x, y
        const int VERTEX_POS_INDX = 0;
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(VERTEX_POS_INDX);
        glVertexAttribPointer(VERTEX_POS_INDX, VERTEX_POS_SIZE, GL_FLOAT, GL_FALSE, 0, v_array);
        
        
        //qDebug()<<m_color;
        glCheckError("render");
        shader->bind();
        glCheckError("render");
        shader->setColor(m_color);
        glCheckError("render");
        shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);
        glCheckError("render");
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        glCheckError("render");
        glDrawArrays(GL_TRIANGLE_FAN,0,size);
        glCheckError("render");
        shader->release();
        glDisableVertexAttribArray(VERTEX_POS_INDX);
    }
}


//=================================================================================================

Q_GLOBAL_STATIC(QnOpenGLRendererManager, qn_OpenGlRenderManager_instance)


QnOpenGLRenderer& QnOpenGLRendererManager::instance(const QGLContext* a_context)
{
    QnOpenGLRendererManager* manager = qn_OpenGlRenderManager_instance();

    QHash<const QGLContext*,QnOpenGLRenderer>::iterator it = manager->getContainer().find(a_context);
    if ( it != manager->getContainer().end() )
        return (*it);

    manager->getContainer().insert(a_context,QnOpenGLRenderer(a_context));
    return *(manager->getContainer().find(a_context));
}