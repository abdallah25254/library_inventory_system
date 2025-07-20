#ifndef LOGIN_WINDOW_H
#define LOGIN_WINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
// Removed QMap and QString includes that are now covered by QtSql or QWidget implicitly.

class LoginWindow : public QWidget {
    Q_OBJECT

public:
    explicit LoginWindow(QWidget* parent = nullptr);
    static void addUser(const QString& username, const QString& password); // Now interacts with DB
    void clearFields();
    static bool userExists(const QString& username); // Now interacts with DB
    QString getLastLoggedInUser() const;

signals:
    void loginSuccessful(const QString& username); // Modified to pass username
    void registerRequested();

private slots:
    void attemptLogin();

private:
    QLineEdit* usernameInput;
    QLineEdit* passwordInput;
    QPushButton* loginButton;
    QPushButton* registerButton;
    QFrame* loginFrame;
    QLabel* iconLabel;
    QLabel* welcomeLabel;

    // Removed static QMap<QString, QString> users; - no longer needed for in-memory storage

    QString lastLoggedInUser; // Store last logged-in user

    void setupUI();
    void applyModernStyling();
    void addShadowEffect(QWidget* widget);
};

#endif // LOGIN_WINDOW_H