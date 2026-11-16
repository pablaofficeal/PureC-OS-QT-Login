#include "session.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtGui/QFontDatabase>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtPlugin>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

Q_IMPORT_PLUGIN(QPurecIntegrationPlugin)

static const char *Style = R"(
QWidget { color: #cdd6f4; }
QLabel#clock { font-size: 64px; font-weight: bold; color: #f5f5fa; }
QLabel#date { font-size: 18px; color: #a6adc8; }
QLabel#host { font-size: 14px; color: #7f849c; }
QLabel#error { color: #f38ba8; font-size: 14px; }
QFrame#card { background: rgba(24, 24, 37, 220); border: 1px solid #45475a; border-radius: 14px; }
QPushButton.user { background: #313244; border: 1px solid #45475a; border-radius: 10px; padding: 10px 16px;
                   font-size: 15px; text-align: left; }
QPushButton.user:checked { background: #3b4a6b; border: 1px solid #89b4fa; }
QPushButton.user:hover { border: 1px solid #89b4fa; }
QLineEdit { background: #11111b; border: 1px solid #45475a; border-radius: 8px; padding: 9px 12px;
            font-size: 16px; color: #f5f5fa; selection-background-color: #89b4fa; }
QLineEdit:focus { border: 1px solid #89b4fa; }
QPushButton#login { background: #89b4fa; color: #11111b; border: none; border-radius: 8px; padding: 10px;
                    font-size: 16px; font-weight: bold; }
QPushButton#login:hover { background: #b4cdfb; }
QPushButton#login:disabled { background: #45475a; color: #7f849c; }
QPushButton.power { background: transparent; border: 1px solid #45475a; border-radius: 8px; padding: 8px 14px;
                    font-size: 14px; color: #bac2de; }
QPushButton.power:hover { border: 1px solid #f38ba8; color: #f38ba8; }
)";

class LoginScreen : public QWidget
{
public:
    LoginScreen()
    {
        setWindowTitle(QStringLiteral("Login"));
        setWindowFlags(Qt::FramelessWindowHint);
        setStyleSheet(QString::fromLatin1(Style));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(32, 28, 32, 24);

        auto *top = new QHBoxLayout;
        auto *brand = new QLabel(QStringLiteral("PureC OS"));
        brand->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #89b4fa;"));
        auto *host = new QLabel(session::hostName());
        host->setObjectName(QStringLiteral("host"));
        top->addWidget(brand);
        top->addStretch();
        top->addWidget(host);
        root->addLayout(top);
        root->addStretch(2);

        m_clock = new QLabel;
        m_clock->setObjectName(QStringLiteral("clock"));
        m_clock->setAlignment(Qt::AlignHCenter);
        m_date = new QLabel;
        m_date->setObjectName(QStringLiteral("date"));
        m_date->setAlignment(Qt::AlignHCenter);
        root->addWidget(m_clock);
        root->addWidget(m_date);
        root->addSpacing(28);

        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("card"));
        card->setFixedWidth(400);
        auto *form = new QVBoxLayout(card);
        form->setContentsMargins(24, 22, 24, 22);
        form->setSpacing(10);

        m_group = new QButtonGroup(this);
        m_group->setExclusive(true);
        const QList<UserEntry> users = session::users();
        for (const UserEntry &u : users) {
            QString label = u.fullName.isEmpty() || u.fullName == u.name
                                ? u.name
                                : QStringLiteral("%1   %2").arg(u.fullName, u.name);
            auto *b = new QPushButton(QStringLiteral("  ") + label);
            b->setProperty("class", QStringLiteral("user"));
            b->setCheckable(true);
            b->setFocusPolicy(Qt::NoFocus);
            b->setProperty("user", u.name);
            m_group->addButton(b);
            form->addWidget(b);
        }
        m_name = new QLineEdit;
        m_name->setPlaceholderText(QStringLiteral("User name"));
        m_name->setVisible(users.isEmpty());
        form->addWidget(m_name);
        m_password = new QLineEdit;
        m_password->setEchoMode(QLineEdit::Password);
        m_password->setPlaceholderText(session::liveSystem() ? QStringLiteral("Live system: no password needed")
                                                             : QStringLiteral("Password"));
        form->addWidget(m_password);
        m_error = new QLabel;
        m_error->setObjectName(QStringLiteral("error"));
        m_error->setAlignment(Qt::AlignHCenter);
        form->addWidget(m_error);
        m_login = new QPushButton(QStringLiteral("Log in"));
        m_login->setObjectName(QStringLiteral("login"));
        m_login->setFocusPolicy(Qt::NoFocus);
        form->addWidget(m_login);

        auto *center = new QHBoxLayout;
        center->addStretch();
        center->addWidget(card);
        center->addStretch();
        root->addLayout(center);
        root->addStretch(3);

        auto *bottom = new QHBoxLayout;
        bottom->addStretch();
        auto *restart = new QPushButton(QStringLiteral("Restart"));
        auto *off = new QPushButton(QStringLiteral("Power off"));
        for (QPushButton *b : {restart, off}) {
            b->setProperty("class", QStringLiteral("power"));
            b->setFocusPolicy(Qt::NoFocus);
            bottom->addWidget(b);
        }
        root->addLayout(bottom);

        connect(m_group, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *) {
            m_error->clear();
            m_password->clear();
            m_password->setFocus();
        });
        if (!m_group->buttons().isEmpty())
            m_group->buttons().first()->setChecked(true);
        connect(m_login, &QPushButton::clicked, this, [this] { tryLogin(); });
        connect(m_password, &QLineEdit::returnPressed, this, [this] { tryLogin(); });
        connect(m_name, &QLineEdit::returnPressed, this, [this] { m_password->setFocus(); });
        connect(restart, &QPushButton::clicked, this, [] { session::power(true); });
        connect(off, &QPushButton::clicked, this, [] { session::power(false); });

        auto *tick = new QTimer(this);
        connect(tick, &QTimer::timeout, this, [this] { updateClock(); });
        tick->start(1000);
        updateClock();
        m_sessionTimer.setInterval(500);
        connect(&m_sessionTimer, &QTimer::timeout, this, [this] { checkSession(); });
        m_password->setFocus();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QLinearGradient g(0, 0, width(), height());
        g.setColorAt(0, QColor(0x11, 0x11, 0x1B));
        g.setColorAt(0.55, QColor(0x1E, 0x1E, 0x2E));
        g.setColorAt(1, QColor(0x24, 0x27, 0x3A));
        p.fillRect(rect(), g);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x89, 0xB4, 0xFA, 18));
        p.drawEllipse(QPointF(width() * 0.82, height() * 0.18), width() * 0.28, width() * 0.28);
        p.setBrush(QColor(0xCB, 0xA6, 0xF7, 14));
        p.drawEllipse(QPointF(width() * 0.12, height() * 0.9), width() * 0.22, width() * 0.22);
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            m_password->clear();
            m_error->clear();
            return;
        }
        if ((e->key() == Qt::Key_Up || e->key() == Qt::Key_Down) && !m_group->buttons().isEmpty()) {
            QList<QAbstractButton *> b = m_group->buttons();
            int i = qMax(0, int(b.indexOf(m_group->checkedButton())));
            i = (i + (e->key() == Qt::Key_Down ? 1 : int(b.size()) - 1)) % int(b.size());
            b[i]->setChecked(true);
            m_password->clear();
            return;
        }
        QWidget::keyPressEvent(e);
    }

private:
    QString selectedUser() const
    {
        if (m_name->isVisible())
            return m_name->text().trimmed();
        QAbstractButton *b = m_group->checkedButton();
        return b ? b->property("user").toString() : QString();
    }

    void updateClock()
    {
        QDateTime now = QDateTime::currentDateTime();
        m_clock->setText(now.toString(QStringLiteral("HH:mm")));
        m_date->setText(now.toString(QStringLiteral("dddd, d MMMM yyyy")));
    }

    void tryLogin()
    {
        if (!m_login->isEnabled())
            return;
        QString error;
        QString user = selectedUser();
        if (!session::authenticate(user, m_password->text(), &error)) {
            m_password->clear();
            m_error->setText(error);
            m_login->setEnabled(false);
            QTimer::singleShot(1500, this, [this] { m_login->setEnabled(true); });
            return;
        }
        m_password->clear();
        m_error->clear();
        int pid = session::start(user, &error);
        if (pid < 0) {
            m_error->setText(error);
            return;
        }
        m_session = pid;
        hide();
        m_sessionTimer.start();
    }

    void checkSession()
    {
        if (m_session <= 0 || !session::finished(m_session))
            return;
        m_session = -1;
        m_sessionTimer.stop();
        showFullScreen();
        m_password->setFocus();
    }

    QLabel *m_clock;
    QLabel *m_date;
    QLabel *m_error;
    QLineEdit *m_name;
    QLineEdit *m_password;
    QPushButton *m_login;
    QButtonGroup *m_group;
    QTimer m_sessionTimer;
    int m_session = -1;
};

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "purec");
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR", "/usr/share/fonts");
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("qt-login"));
    for (const QString &dir : {QStringLiteral("/usr/share/qt-login/fonts"), QStringLiteral("/usr/share/fonts")}) {
        const QDir fonts(dir);
        for (const QString &file : fonts.entryList({QStringLiteral("*.ttf")}, QDir::Files))
            QFontDatabase::addApplicationFont(fonts.filePath(file));
    }
    QFont ui(QStringLiteral("DejaVu Sans"));
    ui.setPixelSize(14);
    QApplication::setFont(ui);
    LoginScreen screen;
    screen.showFullScreen();
    return app.exec();
}
