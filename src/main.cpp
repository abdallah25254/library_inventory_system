#include <QApplication>
#include <QStackedWidget>
#include <QSqlDatabase> // Include QSqlDatabase
#include <QSqlQuery>    // Include QSqlQuery
#include <QSqlError>    // Include QSqlError
#include <QMessageBox>  // For displaying connection errors

#include "login_window.h"
#include "register_window.h"
#include "mainwindow.h"

// Function to establish database connection
bool connectToDatabase() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("library_db.db"); // Set the path to your database file

    if (!db.open()) {
        QMessageBox::critical(nullptr, "Database Error",
            "Failed to connect to database: " + db.lastError().text());
        return false;
    }

    // Optional: Create tables if they don't exist (useful for first run or empty db)
    // You can put your CREATE TABLE statements here, but since you have a .db file,
    // you might not need this on every run.
    QSqlQuery query;

    // Example: Create users table if not exists (only if you intend to manage users via DB)
    // This is just an example, your .db already defines tables.
    // query.exec("CREATE TABLE IF NOT EXISTS users ("
    //            "username VARCHAR(50) PRIMARY KEY,"
    //            "password_hash VARCHAR(255) NOT NULL)");
    // if (query.lastError().isValid()) {
    //     qDebug() << "Error creating users table:" << query.lastError().text();
    // }

    qDebug() << "Database connected successfully!";
    return true;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Establish database connection
    if (!connectToDatabase()) {
        return 1; // Exit if database connection fails
    }

    QStackedWidget* stack = new QStackedWidget();
    stack->setWindowTitle("Library Inventory System");

    LoginWindow* loginWindow = new LoginWindow();
    MainWindow* mainWindow = new MainWindow(loginWindow);
    RegisterWindow* registerWindow = nullptr;

    stack->addWidget(loginWindow);
    stack->addWidget(mainWindow);

    stack->setCurrentWidget(loginWindow);
    stack->resize(480, 650); // Set initial login window size
    stack->show();

    QObject::connect(loginWindow, &LoginWindow::loginSuccessful, [=](const QString& username) {
        mainWindow->setCurrentUser(username.toStdString());
        stack->setCurrentWidget(mainWindow);
        stack->resize(1000, 700);
        stack->setWindowTitle(QString("Library Inventory System - User: %1").arg(username));
        });

    QObject::connect(loginWindow, &LoginWindow::registerRequested, [=]() mutable {
        registerWindow = new RegisterWindow();
        stack->addWidget(registerWindow);
        stack->setCurrentWidget(registerWindow);
        stack->resize(520, 750); // Set register window size
        stack->setWindowTitle("Create Account - Library System");

        QObject::connect(registerWindow, &RegisterWindow::registrationSuccessful, [=]() {
            stack->setCurrentWidget(loginWindow);
            stack->removeWidget(registerWindow);
            delete registerWindow;
            stack->resize(480, 650); // Restore login window size
            stack->setWindowTitle("Library Inventory System");
            });

        QObject::connect(registerWindow, &RegisterWindow::backToLogin, [=]() {
            stack->setCurrentWidget(loginWindow);
            stack->removeWidget(registerWindow);
            delete registerWindow;
            stack->resize(480, 650); // Restore login window size
            stack->setWindowTitle("Library Inventory System");
            });
        });

    QObject::connect(mainWindow, &MainWindow::logoutRequested, [=]() {
        loginWindow->clearFields();
        stack->setCurrentWidget(loginWindow);
        stack->resize(480, 650);
        stack->setWindowTitle("Library Inventory System");
        });

    int result = app.exec();

    // Close database connection when application exits
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    return result;
}