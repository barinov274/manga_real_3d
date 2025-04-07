#include <QOpenGLWidget>
#include <QOpenGLFunctions_1_0>
#include <QMouseEvent>
#include <QImage>
#include <QtMath>
#include <cmath>
#include <QOpenGLTexture>
#include <iostream>
#include <QDir>
#include <QImageReader> // Add this include
#include <QPainter>
#include <QRandomGenerator>
#include <utility>

class BookWidget : public QOpenGLWidget, protected QOpenGLFunctions_1_0
{
    Q_OBJECT

public:
    BookWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent), rotX(0.0f), rotY(0.0f), zoom(5.0f) {
        // Initialize texture IDs array
        for (int i = 0; i < control_tex_len; ++i) {
            textures[i] = nullptr;
        }
    }

    ~BookWidget() {
        makeCurrent(); // Ensure OpenGL context is current
        for (int i = 0; i < control_tex_len; ++i) {
            delete textures[i];
        }
        doneCurrent();
    }

    void loadBook(const QString directoryPath)
    {
        right_to_left=false;

        QStringList imagePaths;
        QDir directory(directoryPath);

        if (!directory.exists()) {
            std::cout << "Directory does not exist:" << directoryPath.toStdString() << std::endl;
            return;
        }

        // Get all supported image extensions from Qt
        QStringList imageExtensions;
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        for (const QByteArray &format : formats) {
            imageExtensions << "*." + QString::fromLatin1(format).toLower();
        }

        // Get all image files (case-insensitive match)
        QFileInfoList files = directory.entryInfoList(
            imageExtensions,
            QDir::Files | QDir::Readable,
            QDir::Name | QDir::IgnoreCase
            );

        // Collect paths
        for (const QFileInfo &file : files) {
            imagePaths.append(file.absoluteFilePath());
        }

        pages = imagePaths;
        /*for(int i=0; i<imagePaths.size()-1; i++){
            pages.append(QImage(imagePaths[i]));
        }
*/

        if (pages.isEmpty()) {
            std::cout << "No images found in directory" << std::endl;
            return;
        }

        bookSize = pages.size()-spec_tex_len-2;

        loadTextures();

        setPage(0);
    }

    void setPage(int page){
        QStringList book_pages = pages.mid(spec_tex_len);

        if(!right_to_left)
            currentPage = page;
        else
            currentPage = book_pages.size()-page-2;

        if(currentPage+1 < book_pages.size()){
            delete textures[control_tex_len-2];
            textures[control_tex_len-2] = new QOpenGLTexture(QImage(book_pages.at(currentPage+1)));
            delete textures[control_tex_len-3];
            textures[control_tex_len-3] = new QOpenGLTexture(QImage(book_pages.at(currentPage)));

            textures[control_tex_len-3]->setMagnificationFilter(QOpenGLTexture::Linear);
            textures[control_tex_len-3]->setMinificationFilter(QOpenGLTexture::Linear);
            textures[control_tex_len-2]->setMagnificationFilter(QOpenGLTexture::Linear);
            textures[control_tex_len-2]->setMinificationFilter(QOpenGLTexture::Linear);
        }

        update();
    }

    int getPageCount(){
        return bookSize;
    }

    void set_right_to_left(){
        right_to_left = !right_to_left;
        QStringList mid = pages.mid(spec_tex_len);
        pages = pages.mid(0,spec_tex_len) + QStringList(mid.rbegin(), mid.rend());
        bookSize = pages.size()-spec_tex_len-2;
        std::swap(textures[1], textures[2]);
        std::swap(textures[6], textures[7]);
        setPage(0);
    }

    void set_soft_cover_z(float z){
        soft_cover_z = z;
        update();
    }

    void set_hide_hyousiura(bool b){
        hide_hyousiura = b;
        update();
    }

    void set_hide_soft_cover(bool b){
        hide_soft_cover=b;
        update();
    }


protected:
    void loadTextures(){
        for (int i = 0; i < control_tex_len-1; ++i) {
            QImage img;
            if(!(pages.size()<control_tex_len))
                img = QImage(pages[i]);
            if (img.isNull()) {
                // Fallback to a solid color if texture fails to load
                img = QImage(64, 64, QImage::Format_RGBA8888);
                img.fill(i == 0 ? Qt::red : i == 1 ? QColorConstants::Svg::orange : i == 2 ? Qt::yellow :
                                            i == 3 ? Qt::green : i == 4 ? Qt::cyan : i==5 ? Qt::blue : QColorConstants::Svg::violet);
            }

            QOpenGLTexture* texture = new QOpenGLTexture(img);
            //texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            texture->setMagnificationFilter(QOpenGLTexture::Linear);
            texture->setMinificationFilter(QOpenGLTexture::Linear);
            delete textures[i];
            textures[i] = texture;
        }
        delete textures[control_tex_len-1];
        textures[control_tex_len-1] = new QOpenGLTexture(draw_stack_texture());
    }

    void initializeGL() override {
        initializeOpenGLFunctions();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D); // Enable 2D texturing
        loadTextures();
    }

    void resizeGL(int w, int h) override {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double aspect = double(w) / double(h);
        double near = 0.1;
        double far = 100.0;
        double fov = 45.0;
        double top = near * std::tan(qDegreesToRadians(fov / 2.0));
        double right = top * aspect;
        glFrustum(-right, right, -top, top, near, far);
        glMatrixMode(GL_MODELVIEW);
    }

    void paintGL() override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -zoom);
        glRotatef(rotX, 1.0f, 0.0f, 0.0f);
        glRotatef(rotY, 0.0f, 1.0f, 0.0f);
        glScalef(4,3,4);
        DrawBook();
    }

    void mousePressEvent(QMouseEvent *event) override {
        startPos = event->pos();
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        QPoint delta = event->pos() - startPos;
        rotX += delta.y() * 0.5f;
        rotY += delta.x() * 0.5f;
        startPos = event->pos();
        update();
        event->accept();
    }

    void wheelEvent(QWheelEvent *event) override {
        zoom -= event->angleDelta().y() / 120.0f * 0.5f;
        if (zoom < 2.0f) zoom = 2.0f;
        if (zoom > 50.0f) zoom = 50.0f;
        update();
        event->accept();
    }

private:
    //QList<QImage> pages;
    QStringList pages;
    float rotX, rotY, zoom;
    QPoint startPos;
    static const int control_tex_len = 11;
    static const int spec_tex_len = 8;
    const GLfloat paper_depth = 0.001;
    QOpenGLTexture* textures[control_tex_len] = {nullptr}; // Array to store texture IDs for 6 faces
    int currentPage = 0;
    int bookSize = 1;
    GLfloat soft_cover_z = 0;
    bool hide_hyousiura = false;
    bool hide_soft_cover = false;

    bool right_to_left = false;

    QImage draw_stack_texture(){
        const int width = 1024;   // Ширина изображения
        const int height = 1024;  // Высота изображения
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(Qt::white);   // Заполняем белым фоном

        QPainter painter(&image);
        painter.setPen(Qt::black); // Цвет линий — чёрный

        const int numPages = bookSize;      // Количество страниц
        const int maxDeviation = 5;    // Максимальное отклонение линии

        for (int i = 1; i < numPages; ++i) {
            int y = i * (height / numPages); // Позиция линии по высоте
            int deviation = QRandomGenerator::global()->bounded(-maxDeviation, maxDeviation); // Случайное отклонение
            painter.drawLine(0, y + deviation, width, y + deviation); // Рисуем линию
        }

        return image.transformed(QMatrix().rotate(90.0));
    }

    void DrawBook(){
        ///Spine
        GLfloat spine_size = bookSize*paper_depth;
        GLfloat spine_radius = spine_size/4;
        GLfloat angle = 90.0f + (180.0f / bookSize) * currentPage;
        GLfloat radian = angle*M_PI/180;

        GLfloat spine_left[2] = {GLfloat(spine_radius*cos(radian)), GLfloat(spine_radius*sin(radian))};
        GLfloat spine_right[2] = {GLfloat(spine_radius*-cos(radian)), GLfloat(spine_radius*-sin(radian))};

        //std::cout << spine_left[0] << " " << spine_left[1] << " " << spine_right[0] << " " << spine_right[1] << std::endl;
        ///Cover
        GLfloat cover_left[2] = {-1+spine_left[0], 0};
        GLfloat cover_right[2] = {1+spine_right[0], 0};

        ///Pages
        GLfloat left_page = -1;
        GLfloat right_page = 1;

        GLfloat hyousiura_left = -1.5+spine_left[0];
        GLfloat hyousiura_right = 1.5+spine_right[0];

        if(!hide_soft_cover){
        ///Soft cover
        //spine
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        textures[0]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(spine_left[0], 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(spine_right[0], -1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(spine_left[0], -1.0f, spine_left[1]-paper_depth-soft_cover_z);
        glEnd();
        textures[0]->release();
        glColor3f(1,1,1);
        glCullFace(GL_FRONT);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(spine_left[0], 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(spine_right[0], -1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(spine_left[0], -1.0f, spine_left[1]-paper_depth-soft_cover_z);
        glEnd();
        glDisable(GL_CULL_FACE);

        //left cover
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        textures[1]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(cover_left[0], 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(spine_left[0], 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(spine_left[0], -1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_left[0], -1.0f, spine_left[1]-paper_depth-soft_cover_z);
        glEnd();
        textures[1]->release();
        glColor3f(1,1,1);
        glCullFace(GL_FRONT);
        glBegin(GL_QUADS);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(cover_left[0], 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(spine_left[0], 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(spine_left[0], -1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_left[0], -1.0f, spine_left[1]-paper_depth-soft_cover_z);
        glEnd();
        glDisable(GL_CULL_FACE);

        //right cover
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        textures[2]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(cover_right[0], 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(spine_right[0], -1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(cover_right[0], -1.0f, spine_right[1]-paper_depth-soft_cover_z);
        glEnd();
        textures[2]->release();
        glColor3f(1,1,1);
        glCullFace(GL_BACK);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(cover_right[0], 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(spine_right[0], -1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(cover_right[0], -1.0f, spine_right[1]-paper_depth-soft_cover_z);
        glEnd();
        glDisable(GL_CULL_FACE);
        if(!hide_hyousiura){
        //hyousiura left
        textures[3]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(cover_left[0], 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(hyousiura_left, 1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(hyousiura_left, -1.0f, spine_left[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(cover_left[0], -1.0f, spine_left[1]-paper_depth-soft_cover_z);
        glEnd();
        //hyousiura right
        textures[4]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(cover_right[0], 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(hyousiura_right, 1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(hyousiura_right, -1.0f, spine_right[1]-paper_depth-soft_cover_z);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_right[0], -1.0f, spine_right[1]-paper_depth-soft_cover_z);
        glEnd();
        }
        }
        ///drawing
        //spine
        textures[5]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(spine_left[0], 1.0f, spine_left[1]);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], 1.0f, spine_right[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(spine_right[0], -1.0f, spine_right[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(spine_left[0], -1.0f, spine_left[1]);
        glEnd();
        //left cover
        textures[6]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(cover_left[0], 1.0f, spine_left[1]);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(spine_left[0], 1.0f, spine_left[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(spine_left[0], -1.0f, spine_left[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_left[0], -1.0f, spine_left[1]);
        glEnd();
        //right cover
        textures[7]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(cover_right[0], 1.0f, spine_right[1]);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], 1.0f, spine_right[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(spine_right[0], -1.0f, spine_right[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(cover_right[0], -1.0f, spine_right[1]);
        glEnd();
        ///between spine
        glBegin(GL_QUADS);
            glVertex3f(spine_left[0],1,spine_left[1]);
            glVertex3f(0,1,spine_radius+paper_depth);
            glVertex3f(0,-1,spine_radius+paper_depth);
            glVertex3f(spine_left[0],-1,spine_left[1]);
        glEnd();
        glBegin(GL_QUADS);
            glVertex3f(spine_right[0],1,spine_right[1]);
            glVertex3f(0,1,spine_radius+paper_depth);
            glVertex3f(0,-1,spine_radius+paper_depth);
            glVertex3f(spine_right[0],-1,spine_right[1]);
        glEnd();

        ///stacks
        //top left stack
        textures[control_tex_len-1]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(0, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_left[0], 1.0f, spine_left[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_left[0], 1.0f, spine_left[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(left_page, 1.0f, spine_radius+paper_depth);
        glEnd();

        //bottom left stack
        textures[control_tex_len-1]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(0, -1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_left[0], -1.0f, spine_left[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_left[0], -1.0f, spine_left[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(left_page, -1.0f, spine_radius+paper_depth);
        glEnd();

        //top right stack
        textures[control_tex_len-1]->bind();
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(0, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], 1.0f, spine_right[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_right[0], 1.0f, spine_right[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(right_page, 1.0f, spine_radius+paper_depth);
        glEnd();

        //bottom right stack
        textures[control_tex_len-1]->bind();
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(0, -1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(spine_right[0], -1.0f, spine_right[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_right[0], -1.0f, spine_right[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(right_page, -1.0f, spine_radius+paper_depth);
        glEnd();

        //right stack
        textures[control_tex_len-1]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(right_page, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(cover_right[0], 1.0f, spine_right[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_right[0], -1.0f, spine_right[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(right_page, -1.0f, spine_radius+paper_depth);
        glEnd();

        //left stack
        textures[control_tex_len-1]->bind();
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(left_page, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(cover_left[0], 1.0f, spine_left[1]);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(cover_left[0], -1.0f, spine_left[1]);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(left_page, -1.0f, spine_radius+paper_depth);
        glEnd();

        ///Pages
        //right page
        textures[control_tex_len-2]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(0, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(right_page, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(right_page, -1.0f, spine_radius+paper_depth);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(0, -1.0f, spine_radius+paper_depth);
        glEnd();
        //left page
        textures[control_tex_len-3]->bind();
        glBegin(GL_QUADS);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(0, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(left_page, 1.0f, spine_radius+paper_depth);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(left_page, -1.0f, spine_radius+paper_depth);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(0, -1.0f, spine_radius+paper_depth);
        glEnd();
    }
};

//#include "main.moc" // Required if using Q_OBJECT in a single-file setup
