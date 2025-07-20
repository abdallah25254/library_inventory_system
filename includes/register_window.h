#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>
#include <QProgressBar>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation> // Make sure this is included for animations

class RegisterWindow : public QWidget {
    Q_OBJECT

public:
    explicit RegisterWindow(QWidget* parent = nullptr);

signals:
    void registrationSuccessful();
    void backToLogin();

private:
    QLineEdit* usernameInput;
    QLineEdit* passwordInput;
    QLineEdit* confirmPasswordInput;
    QPushButton* registerButton;
    QPushButton* backButton;
    QFrame* registerFrame;
    QLabel* passwordStrengthLabel;
    QProgressBar* passwordStrengthBar;

    void setupUI();
    void attemptRegistration();
    void applyModernStyling();
    void addShadowEffect(QWidget* widget);
    void updatePasswordStrength();
    int calculatePasswordStrength(const QString& password);

    // Declare the shakeWidget function
    void shakeWidget(QWidget* widget);
};