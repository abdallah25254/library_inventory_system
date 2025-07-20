#include "register_window.h"
#include "login_window.h" // Assuming LoginWindow methods userExists and addUser are defined here
#include <QMessageBox>
#include <QGridLayout>
#include <QSpacerItem>
#include <QRegularExpression>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <qstyle.h>
#include <qline.h> // Assuming qline.h is meant to be QLine, though not directly used in the provided snippet logic


RegisterWindow::RegisterWindow(QWidget* parent) : QWidget(parent) {
    setupUI();

    // Entrance animation for the register card
    QGraphicsOpacityEffect* opacityEffect = new QGraphicsOpacityEffect(registerFrame);
    registerFrame->setGraphicsEffect(opacityEffect);

    QPropertyAnimation* opacityAnimation = new QPropertyAnimation(opacityEffect, "opacity");
    opacityAnimation->setDuration(600);
    opacityAnimation->setStartValue(0.0);
    opacityAnimation->setEndValue(1.0);
    opacityAnimation->setEasingCurve(QEasingCurve::OutQuad);

    QPoint initialPos = registerFrame->pos();
    QPropertyAnimation* posAnimation = new QPropertyAnimation(registerFrame, "pos");
    posAnimation->setDuration(600);
    posAnimation->setStartValue(QPoint(initialPos.x(), initialPos.y() + 30)); // Start slightly lower
    posAnimation->setEndValue(initialPos);
    posAnimation->setEasingCurve(QEasingCurve::OutQuad);

    opacityAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    posAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void RegisterWindow::setupUI() {
    // Set window properties
    setFixedSize(520, 750);
    setWindowTitle("Create Account - Library System");

    // Create gradient background
    setAttribute(Qt::WA_StyledBackground);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(0);

    // Add top spacer for vertical centering
    mainLayout->addStretch(1);

    // Create register card
    registerFrame = new QFrame(this);
    registerFrame->setObjectName("registerCard");
    registerFrame->setFixedWidth(440);
    registerFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    QVBoxLayout* cardLayout = new QVBoxLayout(registerFrame);
    cardLayout->setContentsMargins(50, 50, 50, 50);
    cardLayout->setSpacing(25);

    // Header section with animated icon
    QVBoxLayout* headerLayout = new QVBoxLayout();
    headerLayout->setSpacing(15);
    headerLayout->setAlignment(Qt::AlignCenter);

    // Modern icon container
    QFrame* iconContainer = new QFrame();
    iconContainer->setObjectName("iconContainer");
    iconContainer->setFixedSize(80, 80);

    QVBoxLayout* iconLayout = new QVBoxLayout(iconContainer);
    iconLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* iconLabel = new QLabel("🆕", this);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 36px; color: #ffffff;");
    iconLayout->addWidget(iconLabel);

    // Title with better typography
    QLabel* titleLabel = new QLabel("Create Account", this);
    titleLabel->setObjectName("mainTitle");
    titleLabel->setAlignment(Qt::AlignCenter);

    // Subtitle
    QLabel* subtitleLabel = new QLabel("Join our library management system", this);
    subtitleLabel->setObjectName("subtitle");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);

    headerLayout->addWidget(iconContainer, 0, Qt::AlignCenter);
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);

    // Input section with floating labels effect
    QVBoxLayout* inputLayout = new QVBoxLayout();
    inputLayout->setSpacing(20);

    // Username field with icon
    QFrame* usernameFrame = new QFrame();
    usernameFrame->setObjectName("inputFrame");
    QHBoxLayout* usernameFrameLayout = new QHBoxLayout(usernameFrame);
    usernameFrameLayout->setContentsMargins(20, 15, 20, 15);
    usernameFrameLayout->setSpacing(15);

    QLabel* userIcon = new QLabel("👤");
    userIcon->setStyleSheet("font-size: 20px; color: #6c757d;");
    userIcon->setFixedSize(24, 24);
    userIcon->setAlignment(Qt::AlignCenter);

    usernameInput = new QLineEdit(this);
    usernameInput->setObjectName("modernInput");
    usernameInput->setPlaceholderText("Choose a unique username");
    usernameInput->setMinimumHeight(25);
    usernameInput->setAttribute(Qt::WA_MacShowFocusRect, false);

    usernameFrameLayout->addWidget(userIcon);
    usernameFrameLayout->addWidget(usernameInput, 1);

    // Password field with icon and strength indicator
    QVBoxLayout* passwordSection = new QVBoxLayout();
    passwordSection->setSpacing(8);

    QFrame* passwordFrame = new QFrame();
    passwordFrame->setObjectName("inputFrame");
    passwordFrame->setMinimumHeight(55); // Ensure consistent height
    QHBoxLayout* passwordFrameLayout = new QHBoxLayout(passwordFrame);
    passwordFrameLayout->setContentsMargins(20, 15, 20, 15);
    passwordFrameLayout->setSpacing(15);

    QLabel* lockIcon = new QLabel("🔒");
    lockIcon->setStyleSheet("font-size: 20px; color: #6c757d;");
    lockIcon->setFixedSize(24, 24);
    lockIcon->setAlignment(Qt::AlignCenter);

    passwordInput = new QLineEdit(this);
    passwordInput->setObjectName("modernInput");
    passwordInput->setPlaceholderText("Create a strong password (min 6 chars)");
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setMinimumHeight(25);
    passwordInput->setAttribute(Qt::WA_MacShowFocusRect, false);
    // Ensure the password input is properly sized
    passwordInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    passwordFrameLayout->addWidget(lockIcon);
    passwordFrameLayout->addWidget(passwordInput, 1);

    // Password strength indicator
    QFrame* strengthFrame = new QFrame();
    strengthFrame->setObjectName("strengthFrame");
    QVBoxLayout* strengthLayout = new QVBoxLayout(strengthFrame);
    strengthLayout->setContentsMargins(20, 12, 20, 12);
    strengthLayout->setSpacing(6);

    passwordStrengthBar = new QProgressBar(this);
    passwordStrengthBar->setMinimum(0);
    passwordStrengthBar->setMaximum(100);
    passwordStrengthBar->setTextVisible(false);
    passwordStrengthBar->setFixedHeight(6);
    passwordStrengthBar->setObjectName("strengthBar");

    passwordStrengthLabel = new QLabel("Enter password to see strength", this);
    passwordStrengthLabel->setObjectName("strengthLabel");
    passwordStrengthLabel->setWordWrap(true);

    strengthLayout->addWidget(passwordStrengthBar);
    strengthLayout->addWidget(passwordStrengthLabel);

    passwordSection->addWidget(passwordFrame);
    passwordSection->addWidget(strengthFrame);

    // Confirm password field with icon
    QFrame* confirmFrame = new QFrame();
    confirmFrame->setObjectName("inputFrame");
    confirmFrame->setMinimumHeight(55); // Ensure consistent height
    QHBoxLayout* confirmFrameLayout = new QHBoxLayout(confirmFrame);
    confirmFrameLayout->setContentsMargins(20, 15, 20, 15);
    confirmFrameLayout->setSpacing(15);

    QLabel* confirmIcon = new QLabel("🔐");
    confirmIcon->setStyleSheet("font-size: 20px; color: #6c757d;");
    confirmIcon->setFixedSize(24, 24);
    confirmIcon->setAlignment(Qt::AlignCenter);

    confirmPasswordInput = new QLineEdit(this);
    confirmPasswordInput->setObjectName("modernInput");
    confirmPasswordInput->setPlaceholderText("Re-enter your password");
    confirmPasswordInput->setEchoMode(QLineEdit::Password);
    confirmPasswordInput->setMinimumHeight(25);
    confirmPasswordInput->setAttribute(Qt::WA_MacShowFocusRect, false);
    // Ensure the confirm password input is properly sized
    confirmPasswordInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    confirmFrameLayout->addWidget(confirmIcon);
    confirmFrameLayout->addWidget(confirmPasswordInput, 1);

    inputLayout->addWidget(usernameFrame);
    inputLayout->addLayout(passwordSection);
    inputLayout->addWidget(confirmFrame);

    // Button section
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(20);

    // Primary register button with gradient
    registerButton = new QPushButton("Create Account", this);
    registerButton->setObjectName("primaryBtn");
    registerButton->setMinimumHeight(55);
    registerButton->setCursor(Qt::PointingHandCursor);

    // Divider with text
    QHBoxLayout* dividerLayout = new QHBoxLayout();
    QFrame* leftLine = new QFrame();
    leftLine->setFrameShape(QFrame::HLine);
    leftLine->setObjectName("dividerLine");

    QLabel* dividerText = new QLabel("or", this);
    dividerText->setObjectName("dividerText");
    dividerText->setAlignment(Qt::AlignCenter);
    dividerText->setFixedWidth(30);

    QFrame* rightLine = new QFrame();
    rightLine->setFrameShape(QFrame::HLine);
    rightLine->setObjectName("dividerLine");

    dividerLayout->addWidget(leftLine, 1);
    dividerLayout->addWidget(dividerText);
    dividerLayout->addWidget(rightLine, 1);

    // Secondary back button
    backButton = new QPushButton("Already have an account? Sign In", this);
    backButton->setObjectName("secondaryBtn");
    backButton->setMinimumHeight(50);
    backButton->setCursor(Qt::PointingHandCursor);

    buttonLayout->addWidget(registerButton);
    buttonLayout->addLayout(dividerLayout);
    buttonLayout->addWidget(backButton);

    // Add all sections to card
    cardLayout->addLayout(headerLayout);
    cardLayout->addLayout(inputLayout);
    cardLayout->addLayout(buttonLayout);

    // Center the card horizontally
    QHBoxLayout* centerLayout = new QHBoxLayout();
    centerLayout->addStretch();
    centerLayout->addWidget(registerFrame);
    centerLayout->addStretch();

    mainLayout->addLayout(centerLayout);
    mainLayout->addStretch(1);

    // Apply modern styling
    applyModernStyling();
    addShadowEffect(registerFrame);

    // Connect signals
    connect(registerButton, &QPushButton::clicked, this, &RegisterWindow::attemptRegistration);
    connect(backButton, &QPushButton::clicked, this, &RegisterWindow::backToLogin);
    connect(passwordInput, &QLineEdit::textChanged, this, &RegisterWindow::updatePasswordStrength);

    // Navigation with Enter/Tab
    connect(usernameInput, &QLineEdit::returnPressed, [this]() {
        passwordInput->setFocus();
        });
    connect(passwordInput, &QLineEdit::returnPressed, [this]() {
        confirmPasswordInput->setFocus();
        });
    connect(confirmPasswordInput, &QLineEdit::returnPressed, this, &RegisterWindow::attemptRegistration);

    // Text change events for focus effects (These are correct and handle the styling)
    connect(usernameInput, &QLineEdit::textChanged, this, [this]() {
        usernameInput->parentWidget()->setProperty("focused", !usernameInput->text().isEmpty() || usernameInput->hasFocus()); // Check if focused or not empty
        usernameInput->parentWidget()->style()->polish(usernameInput->parentWidget());
        });

    connect(passwordInput, &QLineEdit::textChanged, this, [this]() {
        passwordInput->parentWidget()->setProperty("focused", !passwordInput->text().isEmpty() || passwordInput->hasFocus()); // Check if focused or not empty
        passwordInput->parentWidget()->style()->polish(passwordInput->parentWidget());
        });

    connect(confirmPasswordInput, &QLineEdit::textChanged, this, [this]() {
        confirmPasswordInput->parentWidget()->setProperty("focused", !confirmPasswordInput->text().isEmpty() || confirmPasswordInput->hasFocus()); // Check if focused or not empty
        confirmPasswordInput->parentWidget()->style()->polish(confirmPasswordInput->parentWidget());
        });

    // Add connections for actual focus changes using lambda to explicitly check hasFocus()
    connect(usernameInput, &QLineEdit::editingFinished, this, [this]() {
        usernameInput->parentWidget()->setProperty("focused", !usernameInput->text().isEmpty() || usernameInput->hasFocus());
        usernameInput->parentWidget()->style()->polish(usernameInput->parentWidget());
        });
    connect(passwordInput, &QLineEdit::editingFinished, this, [this]() {
        passwordInput->parentWidget()->setProperty("focused", !passwordInput->text().isEmpty() || passwordInput->hasFocus());
        passwordInput->parentWidget()->style()->polish(passwordInput->parentWidget());
        });
    connect(confirmPasswordInput, &QLineEdit::editingFinished, this, [this]() {
        confirmPasswordInput->parentWidget()->setProperty("focused", !confirmPasswordInput->text().isEmpty() || confirmPasswordInput->hasFocus());
        confirmPasswordInput->parentWidget()->style()->polish(confirmPasswordInput->parentWidget());
        });
}

void RegisterWindow::applyModernStyling() {
    QString styleSheet = R"(
        QWidget {
            font-family: 'Segoe UI', 'Roboto', 'SF Pro Display', system-ui, sans-serif;
        }

        RegisterWindow {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #11998e, stop:1 #38ef7d);
        }

        #registerCard {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 24px;
            border: 1px solid rgba(255, 255, 255, 0.2);
        }

        #iconContainer {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #11998e, stop:1 #38ef7d);
            border-radius: 40px;
            border: 3px solid rgba(255, 255, 255, 0.3);
        }

        #mainTitle {
            font-size: 32px;
            font-weight: 700;
            color: #2d3748;
            letter-spacing: -0.5px;
            margin: 5px 0;
        }

        #subtitle {
            font-size: 16px;
            color: #718096;
            font-weight: 400;
            line-height: 1.4;
        }

        #inputFrame {
            background: rgba(247, 250, 252, 0.8);
            border: 2px solid rgba(226, 232, 240, 0.8);
            border-radius: 16px;
            min-height: 55px;
        }

        #inputFrame:hover {
            border-color: rgba(17, 153, 142, 0.4);
            background: rgba(255, 255, 255, 0.9);
        }

        #inputFrame[focused="true"] {
            border-color: #11998e;
            background: rgba(255, 255, 255, 1.0);
            box-shadow: 0 0 0 3px rgba(17, 153, 142, 0.1);
        }

        #strengthFrame {
            background: rgba(247, 250, 252, 0.6);
            border: 1px solid rgba(226, 232, 240, 0.6);
            border-radius: 12px;
            margin-top: 4px;
        }

        #modernInput {
            background: transparent;
            border: none;
            font-size: 16px;
            color: #2d3748;
            font-weight: 500;
            min-height: 25px;
            padding: 0px;
        }

        #modernInput::placeholder {
            color: #a0aec0;
            font-weight: 400;
        }

        #modernInput:focus {
            outline: none;
            border: none;
        }

        QLineEdit[echoMode="2"] {
            lineedit-password-character: 8226;
        }

        #primaryBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #11998e, stop:1 #38ef7d);
            color: white;
            border: none;
            border-radius: 16px;
            font-size: 18px;
            font-weight: 600;
            letter-spacing: 0.5px;
        }

        #primaryBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #0f7c71, stop:1 #2dd36f);
        }

        #primaryBtn:pressed {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #0d6660, stop:1 #25c865);
        }

        #secondaryBtn {
            background: transparent;
            color: #11998e;
            border: 2px solid rgba(17, 153, 142, 0.3);
            border-radius: 16px;
            font-size: 16px;
            font-weight: 600;
        }

        #secondaryBtn:hover {
            background: rgba(17, 153, 142, 0.1);
            border-color: #11998e;
            color: #0f7c71;
        }

        #secondaryBtn:pressed {
            background: rgba(17, 153, 142, 0.2);
        }

        #dividerLine {
            border: none;
            height: 1px;
            background: rgba(203, 213, 224, 0.6);
        }

        #dividerText {
            font-size: 14px;
            color: #a0aec0;
            font-weight: 500;
            background: rgba(255, 255, 255, 0.95);
        }

        #strengthBar {
            border-radius: 3px;
            background: rgba(226, 232, 240, 0.6);
            border: none;
        }

        #strengthBar::chunk {
            border-radius: 3px;
            background: #11998e;
        }

        #strengthLabel {
            font-size: 13px;
            color: #718096;
            font-weight: 500;
        }
    )";

    setStyleSheet(styleSheet);
}

void RegisterWindow::addShadowEffect(QWidget* widget) {
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(40);
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(15);
    shadowEffect->setColor(QColor(17, 153, 142, 80));
    widget->setGraphicsEffect(shadowEffect);
}

void RegisterWindow::updatePasswordStrength() {
    QString password = passwordInput->text();
    int strength = calculatePasswordStrength(password);

    passwordStrengthBar->setValue(strength);

    // Update colors and text based on strength
    QString strengthText;
    QString barColor;
    QString labelColor;

    if (password.isEmpty()) {
        strengthText = "💡 Enter password to see strength";
        barColor = "rgba(226, 232, 240, 0.6)";
        labelColor = "#a0aec0";
    }
    else if (strength < 25) {
        strengthText = "💀 Weak - Too short or simple";
        barColor = "#e53e3e";
        labelColor = "#e53e3e";
    }
    else if (strength < 50) {
        strengthText = "⚠️ Fair - Add numbers or symbols";
        barColor = "#dd6b20";
        labelColor = "#dd6b20";
    }
    else if (strength < 75) {
        strengthText = "⚡ Good - Almost there!";
        barColor = "#d69e2e";
        labelColor = "#d69e2e";
    }
    else {
        strengthText = "✨ Strong - Perfect password!";
        barColor = "#38a169";
        labelColor = "#38a169";
    }

    passwordStrengthLabel->setText(strengthText);
    passwordStrengthLabel->setStyleSheet(QString("color: %1; font-weight: 600;").arg(labelColor));
    passwordStrengthBar->setStyleSheet(QString(
        "#strengthBar::chunk { background: %1; }"
    ).arg(barColor));
}

int RegisterWindow::calculatePasswordStrength(const QString& password) {
    if (password.isEmpty()) return 0;

    int score = 0;

    // Length score (up to 40 points)
    score += qMin(password.length() * 4, 40);

    // Character variety (up to 60 points)
    if (password.contains(QRegularExpression("[a-z]"))) score += 10;
    if (password.contains(QRegularExpression("[A-Z]"))) score += 10;
    if (password.contains(QRegularExpression("[0-9]"))) score += 15;
    if (password.contains(QRegularExpression("[^a-zA-Z0-9]"))) score += 25;

    return qMin(score, 100);
}

// New helper function for shake effect
void RegisterWindow::shakeWidget(QWidget* widget) {
    QPoint originalPos = widget->pos();
    int shakeAmount = 10;
    int duration = 50; // milliseconds per shake segment

    QPropertyAnimation* animation = new QPropertyAnimation(widget, "pos");
    animation->setDuration(duration * 6); // Total duration for 3 shakes
    animation->setLoopCount(1);

    animation->setKeyValueAt(0.0, originalPos);
    animation->setKeyValueAt(0.16, QPoint(originalPos.x() - shakeAmount, originalPos.y()));
    animation->setKeyValueAt(0.33, QPoint(originalPos.x() + shakeAmount, originalPos.y()));
    animation->setKeyValueAt(0.50, QPoint(originalPos.x() - shakeAmount, originalPos.y()));
    animation->setKeyValueAt(0.66, QPoint(originalPos.x() + shakeAmount, originalPos.y()));
    animation->setKeyValueAt(0.83, QPoint(originalPos.x() - shakeAmount / 2, originalPos.y()));
    animation->setKeyValueAt(1.0, originalPos);

    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}


void RegisterWindow::attemptRegistration() {
    QString username = usernameInput->text().trimmed();
    QString password = passwordInput->text();
    QString confirmPassword = confirmPasswordInput->text();

    if (username.isEmpty() || password.isEmpty()) {
        if (username.isEmpty()) shakeWidget(usernameInput->parentWidget());
        if (password.isEmpty()) shakeWidget(passwordInput->parentWidget());
        // Create custom styled message box
        QMessageBox msgBox;
        msgBox.setWindowTitle("Registration Required");
        msgBox.setText("Username and password cannot be empty.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(R"(
            QMessageBox { background: white; font-family: 'Segoe UI', sans-serif; }
            QMessageBox QPushButton { background: #11998e; color: white; border: none; border-radius: 8px; padding: 8px 16px; font-weight: 600; min-width: 80px; }
            QMessageBox QPushButton:hover { background: #0f7c71; }
        )");
        msgBox.exec();
        return;
    }

    if (username.length() < 3) {
        shakeWidget(usernameInput->parentWidget());
        QMessageBox msgBox;
        msgBox.setWindowTitle("Username Too Short");
        msgBox.setText("Username must be at least 3 characters long.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(R"(
            QMessageBox { background: white; font-family: 'Segoe UI', sans-serif; }
            QMessageBox QPushButton { background: #11998e; color: white; border: none; border-radius: 8px; padding: 8px 16px; font-weight: 600; min-width: 80px; }
            QMessageBox QPushButton:hover { background: #0f7c71; }
        )");
        msgBox.exec();
        usernameInput->setFocus();
        return;
    }

    if (password != confirmPassword) {
        shakeWidget(passwordInput->parentWidget());
        shakeWidget(confirmPasswordInput->parentWidget());
        QMessageBox msgBox;
        msgBox.setWindowTitle("Password Mismatch");
        msgBox.setText("Passwords do not match. Please check and try again.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(R"(
            QMessageBox { background: white; font-family: 'Segoe UI', sans-serif; }
            QMessageBox QPushButton { background: #11998e; color: white; border: none; border-radius: 8px; padding: 8px 16px; font-weight: 600; min-width: 80px; }
            QMessageBox QPushButton:hover { background: #0f7c71; }
        )");
        msgBox.exec();
        confirmPasswordInput->setFocus();
        return;
    }

    if (password.length() < 6) {
        shakeWidget(passwordInput->parentWidget());
        QMessageBox msgBox;
        msgBox.setWindowTitle("Password Too Short");
        msgBox.setText("Password must be at least 6 characters long.");
        msgBox.setInformativeText("Please choose a longer password for better security.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(R"(
            QMessageBox { background: white; font-family: 'Segoe UI', sans-serif; }
            QMessageBox QPushButton { background: #11998e; color: white; border: none; border-radius: 8px; padding: 8px 16px; font-weight: 600; min-width: 80px; }
            QMessageBox QPushButton:hover { background: #0f7c71; }
        )");
        msgBox.exec();
        passwordInput->setFocus();
        return;
    }

    // Additional password strength check
    if (calculatePasswordStrength(password) < 25) {
        shakeWidget(passwordInput->parentWidget());
        QMessageBox msgBox;
        msgBox.setWindowTitle("Weak Password");
        msgBox.setText("Your password is too weak.");
        msgBox.setInformativeText("Please choose a stronger password with:\n• At least 6 characters\n• Mix of letters, numbers, and symbols");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(R"(
            QMessageBox { background: white; font-family: 'Segoe UI', sans-serif; }
            QMessageBox QPushButton { background: #11998e; color: white; border: none; border-radius: 8px; padding: 8px 16px; font-weight: 600; min-width: 80px; }
            QMessageBox QPushButton:hover { background: #0f7c71; }
        )");
        msgBox.exec();
        passwordInput->setFocus();
        return;
    }

    if (LoginWindow::userExists(username)) { // Calls static method, which now queries DB
        shakeWidget(usernameInput->parentWidget());
        QMessageBox msgBox;
        msgBox.setWindowTitle("Username Taken");
        msgBox.setText("This username is already taken.");
        msgBox.setInformativeText("Please choose another username.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(R"(
            QMessageBox { background: white; font-family: 'Segoe UI', sans-serif; }
            QMessageBox QPushButton { background: #11998e; color: white; border: none; border-radius: 8px; padding: 8px 16px; font-weight: 600; min-width: 80px; }
            QMessageBox QPushButton:hover { background: #0f7c71; }
        )");
        msgBox.exec();
        usernameInput->setFocus();
        return;
    }

    // Save the user credentials to the database
    LoginWindow::addUser(username, password); // Calls static method, which now writes to DB

    // Add success animation effect
    registerButton->setText("✓ Account Created!");
    registerButton->setStyleSheet(registerButton->styleSheet() +
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #38a169, stop:1 #48bb78);");

    // Success message
    QMessageBox msgBox;
    msgBox.setWindowTitle("Registration Successful");
    msgBox.setText(QString("Welcome, %1! 🎉").arg(username));
    msgBox.setInformativeText("Your account has been created successfully.\nYou can now sign in with your credentials.");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(R"(
        QMessageBox { background: white; font-family: 'Segoe UI', sans-serif; }
        QMessageBox QPushButton { background: #38a169; color: white; border: none; border-radius: 8px; padding: 8px 16px; font-weight: 600; min-width: 80px; }
        QMessageBox QPushButton:hover { background: #2f855a; }
    )");
    msgBox.exec();

    QTimer::singleShot(500, [this]() {
        emit registrationSuccessful();
        });
}