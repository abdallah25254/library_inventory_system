// mainwindow.cpp - REVISED with Enter key default button and auto-tabbing on scan
//old version
#include "mainwindow.h"
#include <QDateTime>
#include <QVBoxLayout>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QString>
#include <QDebug>
#include <QGroupBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QScrollArea>
#include <QTextBrowser>
#include <QInputDialog>
#include <QMenu>
#include "add_supplier_dialog.h" // Assuming this dialog still works and is independent of internal data storage
#include <ctime> // For std::time and std::localtime
#include <algorithm> // For std::sort
#include <set> // For std::set to collect unique users reliably

// For database operations
#include <QSqlDatabase>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFileDialog>
#include <QApplication>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QPageSize>
#include <QTextDocument>
#include <QPageLayout>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // DB migration: add threshold column to existing databases that don't have it yet
    {
        QSqlQuery migration;
        migration.exec("ALTER TABLE items ADD COLUMN threshold INTEGER DEFAULT 5");
    }
    performStartupBackup();   // Auto-backup before anything else touches the DB
    setupUI();
    updateUserDisplay();
    // Show backup result after UI is ready, then low-stock check
    QTimer::singleShot(600, this, &MainWindow::showBackupStartupResult);
    QTimer::singleShot(1400, this, &MainWindow::checkAndNotifyLowStock);
}

void MainWindow::showSuppliersDashboard() {
    bool ok;
    QString password = QInputDialog::getText(this, "Admin Access",
        "Enter admin password:", QLineEdit::Password,
        "", &ok);

    if (ok && !password.isEmpty() && password == "25254") {
        QDialog* dialog = new QDialog(this);
        dialog->setWindowTitle("Suppliers Dashboard");
        dialog->setMinimumSize(1200, 800);
        dialog->setStyleSheet("QDialog { background-color: #f5f5f5; }");

        QHBoxLayout* mainLayout = new QHBoxLayout(dialog);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(20);

        /* LEFT PANEL - ADD TRANSACTION FORM */
        QFrame* formFrame = new QFrame(dialog);
        formFrame->setObjectName("cardFrame");
        formFrame->setFixedWidth(350);

        QVBoxLayout* formLayout = new QVBoxLayout(formFrame);
        formLayout->setContentsMargins(25, 25, 25, 25);
        formLayout->setSpacing(20);

        QLabel* formTitle = new QLabel("Add Supplier Transaction", formFrame);
        formTitle->setObjectName("sectionTitle");
        formLayout->addWidget(formTitle);

        QFormLayout* fieldsLayout = new QFormLayout();
        fieldsLayout->setSpacing(15);

        supplierNameInput = new QLineEdit(formFrame);
        supplierNameInput->setObjectName("styledInput");
        supplierNameInput->setPlaceholderText("Supplier name");
        fieldsLayout->addRow("Supplier Name:", supplierNameInput);

        supplierDateInput = new QDateEdit(QDate::currentDate(), formFrame);
        supplierDateInput->setCalendarPopup(true);
        supplierDateInput->setDisplayFormat("dd/MM/yyyy");
        supplierDateInput->setObjectName("styledInput");
        fieldsLayout->addRow("Date:", supplierDateInput);

        totalAmountInput = new QLineEdit(formFrame);
        totalAmountInput->setObjectName("styledInput");
        totalAmountInput->setPlaceholderText("0.00");
        fieldsLayout->addRow("Total Amount:", totalAmountInput);

        paidAmountInput = new QLineEdit(formFrame);
        paidAmountInput->setObjectName("styledInput");
        paidAmountInput->setPlaceholderText("0.00");
        fieldsLayout->addRow("Paid Amount:", paidAmountInput);

        formLayout->addLayout(fieldsLayout);

        submitSupplierButton = new QPushButton("Add Transaction", formFrame);
        submitSupplierButton->setObjectName("primaryButton");
        submitSupplierButton->setMinimumHeight(45);
        formLayout->addWidget(submitSupplierButton);
        formLayout->addStretch();
        //-----------------------------------------------------------//

        // Set submitSupplierButton as the default button for this dialog
        submitSupplierButton->setDefault(true);

        /* RIGHT PANEL - SUPPLIER TRANSACTIONS */
        QFrame* tabsFrame = new QFrame(dialog);
        tabsFrame->setObjectName("cardFrame");

        QVBoxLayout* tabsLayout = new QVBoxLayout(tabsFrame);
        tabsLayout->setContentsMargins(25, 25, 25, 25);
        tabsLayout->setSpacing(15);

        QPushButton* refreshButton = new QPushButton("🔄 Refresh Dashboard", tabsFrame);
        refreshButton->setObjectName("secondaryButton");
        tabsLayout->addWidget(refreshButton, 0, Qt::AlignRight);

        QLabel* tabsTitle = new QLabel("Supplier Transactions & Payments", tabsFrame);
        tabsTitle->setObjectName("sectionTitle");
        tabsLayout->addWidget(tabsTitle);

        supplierTabWidget = new QTabWidget(tabsFrame); // Assign to member
        supplierTabWidget->setObjectName("supplierTabs");

        auto refreshAllTabs = [=]() {
            supplierTabWidget->clear();
            supplierTables.clear();

            for (const auto& supplier : inventory.getAllSuppliers()) {
                QWidget* supplierTab = new QWidget();
                QVBoxLayout* supplierLayout = new QVBoxLayout(supplierTab);
                supplierLayout->setContentsMargins(10, 10, 10, 10);

                /* SUMMARY CARD */
                double totalRemaining = inventory.getTotalRemainingForSupplier(supplier);
                QFrame* summaryCard = createSummaryCard("Total Remaining",
                    QString("LE%1").arg(totalRemaining, 0, 'f', 2),
                    totalRemaining > 0 ? "#e74c3c" : "#2ecc71");
                summaryCard->setObjectName("summaryCard");
                supplierLayout->addWidget(summaryCard);

                /* PAYMENT FORM */
                QFrame* paymentFrame = new QFrame();
                paymentFrame->setObjectName("cardFrame");
                QHBoxLayout* paymentLayout = new QHBoxLayout(paymentFrame);
                paymentLayout->setContentsMargins(10, 10, 10, 10);

                QDateEdit* paymentDateInput = new QDateEdit(QDate::currentDate(), paymentFrame);
                paymentDateInput->setCalendarPopup(true);
                paymentDateInput->setDisplayFormat("dd/MM/yyyy");
                paymentDateInput->setObjectName("styledInput");

                QLineEdit* paymentAmountInput = new QLineEdit();
                paymentAmountInput->setObjectName("styledInput");
                paymentAmountInput->setPlaceholderText("Payment amount");
                paymentAmountInput->setValidator(new QDoubleValidator(0.0, 999999.99, 2, paymentAmountInput));

                QPushButton* paymentButton = new QPushButton("Add Payment");
                paymentButton->setObjectName("successButton");
                paymentButton->setMinimumHeight(40);

                paymentLayout->addWidget(paymentDateInput);
                paymentLayout->addWidget(paymentAmountInput);
                paymentLayout->addWidget(paymentButton);

                /* TRANSACTIONS TABLE */
                QTableWidget* currentSupplierTable = new QTableWidget();
                currentSupplierTable->setColumnCount(7);
                currentSupplierTable->setHorizontalHeaderLabels({
                    "Date", "Supplier", "Total", "Paid", "Remaining", "Status", "Payment Details"
                    });
                currentSupplierTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
                currentSupplierTable->setColumnWidth(6, 200);
                currentSupplierTable->setAlternatingRowColors(true);
                currentSupplierTable->setSelectionBehavior(QAbstractItemView::SelectRows);
                currentSupplierTable->verticalHeader()->setVisible(false);

                // Modified: Now passes the transactionId to showPaymentHistory
                connect(currentSupplierTable, &QTableWidget::cellDoubleClicked, this,
                    [=](int r, int c) {
                        if (c == 6) {
                            // Fetch the transaction_id from the original data if available
                            // Or, redesign showPaymentHistory to take supplierName and row index,
                            // then find the transaction object from the current transactions vector
                            const auto& transactions = inventory.getSupplierTransactionsForSupplier(supplier);
                            if (r >= 0 && r < transactions.size()) {
                                showPaymentHistory(supplier, transactions[r].transactionId); // Pass the transaction ID
                            }
                        }
                    });


                auto refreshTable = [=]() {
                    const auto& transactions = inventory.getSupplierTransactionsForSupplier(supplier);
                    currentSupplierTable->setRowCount(transactions.size());

                    for (int i = 0; i < transactions.size(); ++i) {
                        const auto& tx = transactions[i];

                        currentSupplierTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(tx.date)));
                        currentSupplierTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(tx.supplierName)));
                        currentSupplierTable->setItem(i, 2, new QTableWidgetItem(QString::number(tx.totalAmount, 'f', 2)));
                        currentSupplierTable->setItem(i, 3, new QTableWidgetItem(QString::number(tx.paidAmount, 'f', 2)));

                        double remaining = tx.getRemainingAmount();
                        QTableWidgetItem* remainingItem = new QTableWidgetItem(QString::number(remaining, 'f', 2));
                        remainingItem->setForeground(remaining > 0 ? Qt::red : Qt::darkGreen);
                        currentSupplierTable->setItem(i, 4, remainingItem);

                        QString status;
                        if (remaining <= 0) status = "✅ Paid in full";
                        else if (tx.paidAmount > 0) status = QString("⏳ %1% paid").arg((tx.paidAmount / tx.totalAmount) * 100, 0, 'f', 1);
                        else status = "❌ Unpaid";
                        currentSupplierTable->setItem(i, 5, new QTableWidgetItem(status));

                        QTableWidgetItem* paymentDetailsItem = new QTableWidgetItem(tx.paymentHistory.empty() ? "Click to view" : "View Payments");
                        paymentDetailsItem->setTextAlignment(Qt::AlignCenter);
                        paymentDetailsItem->setFlags(paymentDetailsItem->flags() & ~Qt::ItemIsEditable);
                        currentSupplierTable->setItem(i, 6, paymentDetailsItem);
                    }
                    };

                // Connect payment button
                connect(paymentButton, &QPushButton::clicked, this, [=]() {
                    addSupplierPayment(supplier, paymentDateInput, paymentAmountInput, summaryCard, currentSupplierTable);
                    });

                refreshTable();
                supplierTables[supplier] = currentSupplierTable;
                supplierLayout->addWidget(currentSupplierTable);
                supplierLayout->addWidget(paymentFrame);
                supplierTabWidget->addTab(supplierTab, QString::fromStdString(supplier));
            }
            };

        refreshAllTabs();
        tabsLayout->addWidget(supplierTabWidget);

        connect(refreshButton, &QPushButton::clicked, refreshAllTabs);

        connect(submitSupplierButton, &QPushButton::clicked, this, [=]() {
            QString name = supplierNameInput->text().trimmed();
            QString date = supplierDateInput->date().toString("dd/MM/yyyy");
            double total = totalAmountInput->text().toDouble();
            double paid = paidAmountInput->text().toDouble();

            if (name.isEmpty()) {
                QMessageBox::warning(this, "Error", "Supplier name is required");
                return;
            }

            if (paid > total) {
                QMessageBox::warning(this, "Error", "Paid amount cannot exceed total");
                return;
            }

            inventory.addSupplierTransaction(date.toStdString(), name.toStdString(), total, paid);

            refreshAllTabs(); // Re-fetch all suppliers and their data
            supplierNameInput->clear();
            totalAmountInput->clear();
            paidAmountInput->clear();
            });

        mainLayout->addWidget(formFrame);
        mainLayout->addWidget(tabsFrame);

        QPushButton* closeButton = new QPushButton("Close Dashboard", dialog);
        closeButton->setObjectName("dangerButton");
        connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);
        tabsLayout->addWidget(closeButton, 0, Qt::AlignRight);

        dialog->exec();
        delete dialog;
    }
    else if (ok) {
        QMessageBox::warning(this, "Access Denied", "Incorrect password!");
    }
}

QFrame* MainWindow::createSummaryCard(const QString& title, const QString& value, const QString& color) {
    QFrame* card = new QFrame();
    card->setStyleSheet(QString(
        "background-color: %1;"
        "border-radius: 8px;"
        "padding: 15px;"
        "color: white;"
    ).arg(color));

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setSpacing(5);

    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");

    QLabel* valueLabel = new QLabel(value);
    valueLabel->setObjectName("valueLabel");
    valueLabel->setStyleSheet("font-size: 24px; font-weight: bold;");

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);

    return card;
}

void MainWindow::showTotalSalesHistory() {
    bool ok;
    QString password = QInputDialog::getText(this, "Admin Access",
        "Enter admin password:", QLineEdit::Password,
        "", &ok);

    if (ok && !password.isEmpty()) {
        if (password == "25254") {
            QDialog dialog(this);
            dialog.setWindowTitle("Admin Sales & Returns Dashboard");
            dialog.setMinimumSize(1000, 800);
            dialog.setStyleSheet("QDialog { background-color: #f5f5f5; }");

            QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
            mainLayout->setContentsMargins(15, 15, 15, 15);
            mainLayout->setSpacing(15);

            QTabWidget* mainDashboardTabWidget = new QTabWidget(); // Renamed to avoid confusion with member tabWidget
            mainDashboardTabWidget->setStyleSheet(R"(
                QTabWidget::pane { border: 1px solid #ddd; }
                QTabBar::tab {
                    padding: 8px 12px;
                    background: #f1f1f1;
                    border: 1px solid #ddd;
                }
                QTabBar::tab:selected {
                    background: #fff;
                    border-bottom: 2px solid #3498db;
                }
            )");

            // Summary Tab
            QWidget* summaryTab = new QWidget();
            QVBoxLayout* summaryLayout = new QVBoxLayout(summaryTab);
            summaryLayout->setContentsMargins(10, 10, 10, 10);

            QLabel* summaryTitle = new QLabel("📊 Sales & Returns Summary");
            summaryTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");

            // Daily Summary Controls
            QFrame* dailySummaryControlsFrame = new QFrame();
            dailySummaryControlsFrame->setObjectName("cardFrame");
            QVBoxLayout* dailySummaryLayout = new QVBoxLayout(dailySummaryControlsFrame);
            dailySummaryLayout->setContentsMargins(15, 15, 15, 15);
            dailySummaryLayout->setSpacing(10);

            QLabel* dailySummaryLabel = new QLabel("Daily Summary Date Range:");
            dailySummaryLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
            dailySummaryLayout->addWidget(dailySummaryLabel);

            QHBoxLayout* dailyDateFilterLayout = new QHBoxLayout();
            QLabel* dailyStartDateLabel = new QLabel("From:");
            QDateEdit* dailySummaryStartDate = new QDateEdit(QDate::currentDate().addMonths(-1));
            dailySummaryStartDate->setCalendarPopup(true);
            dailySummaryStartDate->setDisplayFormat("yyyy-MM-dd");
            dailySummaryStartDate->setObjectName("styledInput");

            QLabel* dailyEndDateLabel = new QLabel("To:");
            QDateEdit* dailySummaryEndDate = new QDateEdit(QDate::currentDate());
            dailySummaryEndDate->setCalendarPopup(true);
            dailySummaryEndDate->setDisplayFormat("yyyy-MM-dd");
            dailySummaryEndDate->setObjectName("styledInput");

            QPushButton* dailySummaryApplyButton = new QPushButton("Apply Daily Filter");
            dailySummaryApplyButton->setObjectName("primaryButton");
            dailySummaryApplyButton->setMinimumHeight(35);
            dailySummaryApplyButton->setCursor(Qt::PointingHandCursor);

            dailyDateFilterLayout->addWidget(dailyStartDateLabel);
            dailyDateFilterLayout->addWidget(dailySummaryStartDate);
            dailyDateFilterLayout->addSpacing(10);
            dailyDateFilterLayout->addWidget(dailyEndDateLabel);
            dailyDateFilterLayout->addWidget(dailySummaryEndDate);
            dailyDateFilterLayout->addStretch();
            dailySummaryLayout->addLayout(dailyDateFilterLayout);
            dailySummaryLayout->addWidget(dailySummaryApplyButton, 0, Qt::AlignRight);

            summaryLayout->addWidget(dailySummaryControlsFrame);

            QTableWidget* dailySummaryTable = new QTableWidget();
            dailySummaryTable->setColumnCount(5); // Date, Sales Count, Sales Revenue, Returns Count, Refund Amount
            dailySummaryTable->setHorizontalHeaderLabels({ "Date", "Sales Count", "Sales Revenue", "Returns Count", "Refund Amount" });
            dailySummaryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            dailySummaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
            dailySummaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
            dailySummaryTable->setStyleSheet(R"(
                QTableWidget {
                    background-color: white;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                }
                QHeaderView::section {
                    background-color: #f2f2f2;
                    padding: 8px;
                    border: none;
                }
            )");
            summaryLayout->addWidget(dailySummaryTable);

            // Grand Totals Summary Cards
            QHBoxLayout* cardsLayout = new QHBoxLayout();
            cardsLayout->setSpacing(15);

            QFrame* netRevenueCard = createSummaryCard("💰 Net Revenue", "LE0.00", "#28a745");
            QFrame* grossSalesCard = createSummaryCard("📈 Gross Sales", "LE0.00", "#007bff");
            QFrame* refundsCard = createSummaryCard("📉 Refunds", "LE0.00", "#dc3545");
            QFrame* totalSalesCountCard = createSummaryCard("🧾 Total Sales Count", "0", "#ffc107");
            QFrame* totalReturnsCountCard = createSummaryCard("↩️ Total Returns Count", "0", "#6c757d");

            cardsLayout->addWidget(netRevenueCard);
            cardsLayout->addWidget(grossSalesCard);
            cardsLayout->addWidget(refundsCard);
            cardsLayout->addWidget(totalSalesCountCard);
            cardsLayout->addWidget(totalReturnsCountCard);

            // Existing user breakdown table and its title
            QLabel* userBreakdownTitle = new QLabel("User Breakdown (Sales & Returns)");
            userBreakdownTitle->setStyleSheet("font-size: 16px; font-weight: bold; margin-top: 20px; margin-bottom: 10px;");

            QTableWidget* userSummaryTable = new QTableWidget(); // Renamed to avoid conflict
            userSummaryTable->setColumnCount(7);
            userSummaryTable->setHorizontalHeaderLabels({ "User", "Sales Count", "Sales Revenue", "Returns Count", "Refund Amount", "Net Revenue", "Items Transacted" });
            userSummaryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            userSummaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
            userSummaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
            userSummaryTable->setStyleSheet(R"(
                QTableWidget {
                    background-color: white;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                }
                QHeaderView::section {
                    background-color: #f2f2f2;
                    padding: 8px;
                    border: none;
                }
            )");

            // Function to refresh the daily summary table AND update summary cards AND user breakdown table
            auto refreshDailySummaryTableAndUpdateCards = [=]() {
                dailySummaryTable->setRowCount(0);
                std::map<QDate, std::tuple<double, int, double, int>> dailyAggregates; // salesTotal, salesCount, refundTotal, returnsCount

                double currentGrandTotalSales = 0.0; // To sum for summary cards
                double currentGrandTotalRefunds = 0.0; // To sum for summary cards
                int currentTotalSalesCount = 0; // To sum for summary cards
                int currentTotalReturnsCount = 0; // To sum for summary cards

                // User Breakdown data for the filtered date range
                std::map<std::string, double> userSalesTotals;
                std::map<std::string, int> userSalesCounts;
                std::map<std::string, double> userRefundTotals;
                std::map<std::string, int> userReturnsCounts;
                std::map<std::string, int> userItemsSoldCounts;
                std::map<std::string, int> userItemsReturnedCounts;

                // Iterate through all sales and returns, then aggregate by date and user
                QSqlQuery salesQuery;
                salesQuery.prepare("SELECT sale_date, username, total_amount, sale_number FROM sales WHERE sale_date BETWEEN :start_date AND :end_date");
                salesQuery.bindValue(":start_date", dailySummaryStartDate->date().toString("yyyy-MM-dd"));
                salesQuery.bindValue(":end_date", dailySummaryEndDate->date().toString("yyyy-MM-dd"));

                if (salesQuery.exec()) {
                    while (salesQuery.next()) {
                        QDate saleDate = salesQuery.value("sale_date").toDate();
                        std::string username = salesQuery.value("username").toString().toStdString();
                        double totalAmount = salesQuery.value("total_amount").toDouble();
                        int saleNumber = salesQuery.value("sale_number").toInt();

                        dailyAggregates[saleDate] = std::make_tuple(
                            std::get<0>(dailyAggregates[saleDate]) + totalAmount,
                            std::get<1>(dailyAggregates[saleDate]) + 1,
                            std::get<2>(dailyAggregates[saleDate]),
                            std::get<3>(dailyAggregates[saleDate])
                        );
                        currentGrandTotalSales += totalAmount;
                        currentTotalSalesCount++;

                        userSalesTotals[username] += totalAmount;
                        userSalesCounts[username]++;

                        // Get items sold for this sale to count total items transacted
                        QSqlQuery saleItemsQuery;
                        saleItemsQuery.prepare("SELECT quantity FROM sale_items WHERE sale_number = :sale_number");
                        saleItemsQuery.bindValue(":sale_number", saleNumber);
                        if (saleItemsQuery.exec()) {
                            while (saleItemsQuery.next()) {
                                userItemsSoldCounts[username] += saleItemsQuery.value("quantity").toInt();
                            }
                        }
                    }
                }
                else {
                    qDebug() << "Error fetching sales for summary:" << salesQuery.lastError().text();
                }

                QSqlQuery returnsQuery;
                returnsQuery.prepare("SELECT return_date, username, refund_amount, quantity FROM returns_history WHERE return_date BETWEEN :start_date AND :end_date");
                returnsQuery.bindValue(":start_date", dailySummaryStartDate->date().toString("yyyy-MM-dd"));
                returnsQuery.bindValue(":end_date", dailySummaryEndDate->date().toString("yyyy-MM-dd"));

                if (returnsQuery.exec()) {
                    while (returnsQuery.next()) {
                        QDate returnDate = returnsQuery.value("return_date").toDate();
                        std::string username = returnsQuery.value("username").toString().toStdString();
                        double refundAmount = returnsQuery.value("refund_amount").toDouble();
                        int quantity = returnsQuery.value("quantity").toInt();

                        dailyAggregates[returnDate] = std::make_tuple(
                            std::get<0>(dailyAggregates[returnDate]),
                            std::get<1>(dailyAggregates[returnDate]),
                            std::get<2>(dailyAggregates[returnDate]) + refundAmount,
                            std::get<3>(dailyAggregates[returnDate]) + 1
                        );
                        currentGrandTotalRefunds += refundAmount;
                        currentTotalReturnsCount++;

                        userRefundTotals[username] += refundAmount;
                        userReturnsCounts[username]++;
                        userItemsReturnedCounts[username] += quantity;
                    }
                }
                else {
                    qDebug() << "Error fetching returns for summary:" << returnsQuery.lastError().text();
                }

                std::vector<std::pair<QDate, std::tuple<double, int, double, int>>> sortedDailyAggregates(dailyAggregates.begin(), dailyAggregates.end());
                std::sort(sortedDailyAggregates.begin(), sortedDailyAggregates.end(), [](const auto& a, const auto& b) {
                    return a.first < b.first;
                    });

                dailySummaryTable->setRowCount(sortedDailyAggregates.size());
                int row = 0;
                for (const auto& entry : sortedDailyAggregates) {
                    QDate date = entry.first;
                    double salesTotal = std::get<0>(entry.second);
                    int salesCount = std::get<1>(entry.second);
                    double refundTotal = std::get<2>(entry.second);
                    int returnsCount = std::get<3>(entry.second);

                    dailySummaryTable->setItem(row, 0, new QTableWidgetItem(date.toString("yyyy-MM-dd")));
                    dailySummaryTable->setItem(row, 1, new QTableWidgetItem(QString::number(salesCount)));
                    dailySummaryTable->setItem(row, 2, new QTableWidgetItem(QString("LE%1").arg(salesTotal, 0, 'f', 2)));
                    dailySummaryTable->setItem(row, 3, new QTableWidgetItem(QString::number(returnsCount)));
                    dailySummaryTable->setItem(row, 4, new QTableWidgetItem(QString("LE%1").arg(refundTotal, 0, 'f', 2)));
                    row++;
                }

                // Update the summary cards based on the filtered date range
                QLabel* netRevenueLabel = netRevenueCard->findChild<QLabel*>("valueLabel");
                if (netRevenueLabel) {
                    double netRevenue = currentGrandTotalSales - currentGrandTotalRefunds;
                    netRevenueLabel->setText(QString("LE%1").arg(netRevenue, 0, 'f', 2));
                    netRevenueCard->setStyleSheet(QString(
                        "background-color: %1;"
                        "border-radius: 8px;"
                        "padding: 15px;"
                        "color: white;"
                    ).arg(netRevenue >= 0 ? "#28a745" : "#dc3545")); // Green for profit, red for loss
                }

                QLabel* grossSalesLabel = grossSalesCard->findChild<QLabel*>("valueLabel");
                if (grossSalesLabel) {
                    grossSalesLabel->setText(QString("LE%1").arg(currentGrandTotalSales, 0, 'f', 2));
                }

                QLabel* refundsLabel = refundsCard->findChild<QLabel*>("valueLabel");
                if (refundsLabel) {
                    refundsLabel->setText(QString("LE%1").arg(currentGrandTotalRefunds, 0, 'f', 2));
                }

                QLabel* totalSalesCountLabel = totalSalesCountCard->findChild<QLabel*>("valueLabel");
                if (totalSalesCountLabel) {
                    totalSalesCountLabel->setText(QString::number(currentTotalSalesCount));
                }

                QLabel* totalReturnsCountLabel = totalReturnsCountCard->findChild<QLabel*>("valueLabel");
                if (totalReturnsCountLabel) {
                    totalReturnsCountLabel->setText(QString::number(currentTotalReturnsCount));
                }

                // Refresh User Breakdown Table based on the same date filter
                std::vector<std::string> allUsers = inventory.getAllUsers();
                userSummaryTable->setRowCount(allUsers.size());
                int userRow = 0;
                for (const auto& username : allUsers) {
                    double salesTotal = userSalesTotals[username];
                    int salesCount = userSalesCounts[username];
                    double refundTotal = userRefundTotals[username];
                    int returnsCount = userReturnsCounts[username];
                    double userNetRevenue = salesTotal - refundTotal;
                    int userItemsTransacted = userItemsSoldCounts[username] + userItemsReturnedCounts[username];


                    userSummaryTable->setItem(userRow, 0, new QTableWidgetItem(QString::fromStdString(username)));
                    userSummaryTable->setItem(userRow, 1, new QTableWidgetItem(QString::number(salesCount)));
                    userSummaryTable->setItem(userRow, 2, new QTableWidgetItem(QString("LE%1").arg(salesTotal, 0, 'f', 2)));
                    userSummaryTable->setItem(userRow, 3, new QTableWidgetItem(QString::number(returnsCount)));
                    userSummaryTable->setItem(userRow, 4, new QTableWidgetItem(QString("-LE%1").arg(refundTotal, 0, 'f', 2)));
                    userSummaryTable->setItem(userRow, 5, new QTableWidgetItem(QString("LE%1").arg(userNetRevenue, 0, 'f', 2)));
                    userSummaryTable->setItem(userRow, 6, new QTableWidgetItem(QString::number(userItemsTransacted)));
                    userRow++;
                }
                };

            // Connect daily summary apply button
            connect(dailySummaryApplyButton, &QPushButton::clicked, refreshDailySummaryTableAndUpdateCards);
            refreshDailySummaryTableAndUpdateCards(); // Initial refresh to populate summary cards and daily table

            summaryLayout->addWidget(summaryTitle);
            summaryLayout->addLayout(cardsLayout); // Add the summary cards layout
            summaryLayout->addWidget(userBreakdownTitle);
            summaryLayout->addWidget(userSummaryTable);

            // Detailed Tab - Now with QTabWidget for each user
            QWidget* detailedMainTab = new QWidget(); // Main container for detailed view
            QVBoxLayout* detailedMainLayout = new QVBoxLayout(detailedMainTab);
            detailedMainLayout->setContentsMargins(10, 10, 10, 10);
            detailedMainLayout->setSpacing(15);

            QLabel* detailedTitle = new QLabel("Detailed Sales & Returns History by User");
            detailedTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 5px;");
            detailedMainLayout->addWidget(detailedTitle);

            // Search and Date Filter controls (global for all user tabs)
            QFrame* filterFrame = new QFrame();
            filterFrame->setObjectName("cardFrame");
            QVBoxLayout* filterLayout = new QVBoxLayout(filterFrame);
            filterLayout->setContentsMargins(15, 15, 15, 15);
            filterLayout->setSpacing(10);

            QLabel* filterLabel = new QLabel("Filter Current User's History:");
            filterLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
            filterLayout->addWidget(filterLabel);

            QHBoxLayout* searchInputFilterLayout = new QHBoxLayout();
            salesHistorySearchInput = new QLineEdit();
            salesHistorySearchInput->setObjectName("styledInput");
            salesHistorySearchInput->setPlaceholderText("Search by Item ID or Name...");
            searchInputFilterLayout->addWidget(salesHistorySearchInput);
            filterLayout->addLayout(searchInputFilterLayout);

            QHBoxLayout* dateFilterLayout = new QHBoxLayout();
            QLabel* startDateLabel = new QLabel("From:");
            salesHistoryStartDate = new QDateEdit(QDate::currentDate().addYears(-1));
            salesHistoryStartDate->setCalendarPopup(true);
            salesHistoryStartDate->setDisplayFormat("yyyy-MM-dd");
            salesHistoryStartDate->setObjectName("styledInput");

            QLabel* endDateLabel = new QLabel("To:");
            salesHistoryEndDate = new QDateEdit(QDate::currentDate());
            salesHistoryEndDate->setCalendarPopup(true);
            salesHistoryEndDate->setDisplayFormat("yyyy-MM-dd");
            salesHistoryEndDate->setObjectName("styledInput");

            dateFilterLayout->addWidget(startDateLabel);
            dateFilterLayout->addWidget(salesHistoryStartDate);
            dateFilterLayout->addSpacing(10);
            dateFilterLayout->addWidget(endDateLabel);
            dateFilterLayout->addWidget(salesHistoryEndDate);
            dateFilterLayout->addStretch();

            salesHistoryApplyFilterButton = new QPushButton("Apply Filter");
            salesHistoryApplyFilterButton->setObjectName("primaryButton");
            salesHistoryApplyFilterButton->setMinimumHeight(35);
            salesHistoryApplyFilterButton->setCursor(Qt::PointingHandCursor);

            filterLayout->addLayout(dateFilterLayout);
            filterLayout->addWidget(salesHistoryApplyFilterButton, 0, Qt::AlignRight);
            detailedMainLayout->addWidget(filterFrame);

            // NEW: QTabWidget for user-specific history tabs
            userHistoryTabWidget = new QTabWidget(); // Assign to member variable
            userHistoryTabWidget->setObjectName("userHistoryTabs");
            userHistoryTabWidget->setStyleSheet(R"(
                QTabWidget#userHistoryTabs::pane { border: 1px solid #ddd; }
                QTabWidget#userHistoryTabs QTabBar::tab {
                    background: #f8f8f8;
                    color: #555;
                    padding: 8px 15px;
                    border: 1px solid #ddd;
                    border-bottom: none;
                    border-top-left-radius: 4px;
                    border-top-right-radius: 4px;
                }
                QTabWidget#userHistoryTabs QTabBar::tab:selected {
                    background: #ffffff;
                    color: #007bff;
                    border-bottom: 2px solid #007bff;
                    font-weight: bold;
                }
            )");

            userHistoryTables.clear(); // Clear any previous tables

            // Populate user-specific tabs
            for (const auto& username : inventory.getAllUsers()) { // Get users from DB
                QWidget* userPage = new QWidget(); //
                QVBoxLayout* userPageLayout = new QVBoxLayout(userPage); //
                userPageLayout->setContentsMargins(10, 10, 10, 10); //

                QTableWidget* userTable = new QTableWidget(); //
                userTable->setObjectName("styledTable"); //
                userTable->setColumnCount(8); // Date, Type, Item ID, Item Name, Quantity, Amount, Discount, Details
                userTable->setHorizontalHeaderLabels({ //
                    "Date", "Type", "Item ID", "Item Name", "Quantity", "Amount", "Discount", "Details" //
                    });
                userTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); //
                userTable->setColumnWidth(7, 150); // Adjust details column width
                userTable->setAlternatingRowColors(true); //
                userTable->setSelectionBehavior(QAbstractItemView::SelectRows); //
                userTable->verticalHeader()->setVisible(false); //

                userPageLayout->addWidget(userTable); //
                userHistoryTables[username] = userTable; // Store table pointer

                userHistoryTabWidget->addTab(userPage, QString::fromStdString(username)); //
            }
            detailedMainLayout->addWidget(userHistoryTabWidget); //

            // Connect signals for filtering (when tab changes, or filter button clicked)
            connect(salesHistoryApplyFilterButton, &QPushButton::clicked, this, &MainWindow::searchSalesReturnsHistory); //
            connect(userHistoryTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onUserHistoryTabChanged); //

            // Initial refresh of the currently active detailed user tab
            onUserHistoryTabChanged(userHistoryTabWidget->currentIndex()); //

            mainDashboardTabWidget->addTab(summaryTab, "📊 Summary"); //
            mainDashboardTabWidget->addTab(detailedMainTab, "📝 Detailed View"); // Add the new detailed main tab

            QPushButton* closeButton = new QPushButton("Close");
            closeButton->setStyleSheet(R"(
                QPushButton {
                    padding: 8px 16px;
                    font-size: 14px;
                    background-color: #3498db;
                    color: white;
                    border: none;
                    border-radius: 4px;
                    min-width: 80px;
                }
                QPushButton:hover {
                    background-color: #2980b9;
                }
            )");
            connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

            mainLayout->addWidget(mainDashboardTabWidget); // Add the main dashboard tab widget
            mainLayout->addWidget(closeButton, 0, Qt::AlignRight);

            dialog.exec();
        }
        else {
            QMessageBox::warning(this, "Access Denied", "Incorrect password!");
        }
    }
}
// NEW: Slot to handle tab changes in the user history tab widget
void MainWindow::onUserHistoryTabChanged(int index) { //
    if (userHistoryTabWidget && index >= 0 && index < userHistoryTabWidget->count()) { //
        QString username = userHistoryTabWidget->tabText(index); //
        // Refresh the newly selected tab's content with current filter settings
        refreshDetailedSalesReturnsTable(username.toStdString(), salesHistorySearchInput->text().trimmed(), salesHistoryStartDate->date(), salesHistoryEndDate->date()); //
    }
}


// NEW: Slot to apply search and date filters for sales/returns history
void MainWindow::searchSalesReturnsHistory() { //
    QString searchTerm = salesHistorySearchInput->text().trimmed(); //
    QDate startDate = salesHistoryStartDate->date(); //
    QDate endDate = salesHistoryEndDate->date(); //

    // Get the currently active user tab
    if (userHistoryTabWidget && userHistoryTabWidget->currentIndex() != -1) { //
        QString username = userHistoryTabWidget->tabText(userHistoryTabWidget->currentIndex()); //
        refreshDetailedSalesReturnsTable(username.toStdString(), searchTerm, startDate, endDate); //
    }
}

// NEW: Refresh the detailed sales/returns history table with filters for a SPECIFIC USER
void MainWindow::refreshDetailedSalesReturnsTable(const std::string& username, const QString& searchTerm, const QDate& startDate, const QDate& endDate) { //
    QTableWidget* targetTable = userHistoryTables[username]; //
    if (!targetTable) return; // Should not happen if map is correctly populated

    targetTable->setRowCount(0); // Clear existing rows

    // Set the correct column count for the table (including the new "Discount" column)
    targetTable->setColumnCount(8); // Date, Type, Item ID, Item Name, Quantity, Amount, Discount, Details
    targetTable->setHorizontalHeaderLabels({ //
        "Date", "Type", "Item ID", "Item Name", "Quantity", "Amount", "Discount", "Details" //
        });
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); //
    targetTable->setColumnWidth(7, 150); // Adjust details column width
    targetTable->setAlternatingRowColors(true); //
    targetTable->setSelectionBehavior(QAbstractItemView::SelectRows); //
    targetTable->verticalHeader()->setVisible(false); //

    QString lowerSearchTerm = searchTerm.toLower(); //

    std::vector<std::tuple<QDate, QString, QString, QString, int, double, QString, QString>> userHistoryRecords; //

    // Handle Sales for this user from DB
    // NOTE: This assumes Sale struct now includes username for filtering
    // and that item details are fetched per sale.
    QSqlQuery salesQuery;
    salesQuery.prepare("SELECT sale_number, sale_date, total_amount FROM sales WHERE username = :username AND sale_date BETWEEN :start_date AND :end_date");
    salesQuery.bindValue(":username", QString::fromStdString(username));
    salesQuery.bindValue(":start_date", startDate.toString("yyyy-MM-dd"));
    salesQuery.bindValue(":end_date", endDate.toString("yyyy-MM-dd"));

    if (salesQuery.exec()) {
        while (salesQuery.next()) {
            int saleNumber = salesQuery.value("sale_number").toInt();
            QDate saleDate = salesQuery.value("sale_date").toDate();

            // Fetch items for this sale
            QSqlQuery saleItemsQuery;
            saleItemsQuery.prepare("SELECT item_id, item_name, quantity, total_price, discount_percent FROM sale_items WHERE sale_number = :sale_number");
            saleItemsQuery.bindValue(":sale_number", saleNumber);
            if (saleItemsQuery.exec()) {
                while (saleItemsQuery.next()) {
                    QString itemId = saleItemsQuery.value("item_id").toString();
                    QString itemName = saleItemsQuery.value("item_name").toString();
                    int quantity = saleItemsQuery.value("quantity").toInt();
                    double totalPrice = saleItemsQuery.value("total_price").toDouble();
                    double discountPercent = saleItemsQuery.value("discount_percent").toDouble();

                    bool itemMatch = lowerSearchTerm.isEmpty() ||
                        itemId.toLower().contains(lowerSearchTerm) ||
                        itemName.toLower().contains(lowerSearchTerm);

                    if (itemMatch) {
                        QString itemDiscountStatus;
                        if (discountPercent > 0) {
                            itemDiscountStatus = QString("%1%").arg(discountPercent, 0, 'f', 1);
                        }
                        else {
                            itemDiscountStatus = "No";
                        }
                        userHistoryRecords.emplace_back(
                            saleDate,
                            "Sale",
                            itemId,
                            itemName,
                            quantity,
                            totalPrice,
                            itemDiscountStatus,
                            QString("Sale #%1").arg(saleNumber)
                        );
                    }
                }
            }
            else {
                qDebug() << "Error fetching sale items for detailed history:" << saleItemsQuery.lastError().text();
            }
        }
    }
    else {
        qDebug() << "Error fetching sales for detailed history:" << salesQuery.lastError().text();
    }


    // Handle Returns for this user from DB
    QSqlQuery returnsQuery;
    returnsQuery.prepare("SELECT return_date, item_id, item_name, quantity, refund_amount, reason FROM returns_history WHERE username = :username AND return_date BETWEEN :start_date AND :end_date");
    returnsQuery.bindValue(":username", QString::fromStdString(username));
    returnsQuery.bindValue(":start_date", startDate.toString("yyyy-MM-dd"));
    returnsQuery.bindValue(":end_date", endDate.toString("yyyy-MM-dd"));

    if (returnsQuery.exec()) {
        while (returnsQuery.next()) {
            QDate returnDate = returnsQuery.value("return_date").toDate();
            QString itemId = returnsQuery.value("item_id").toString();
            QString itemName = returnsQuery.value("item_name").toString();
            int quantity = returnsQuery.value("quantity").toInt();
            double refundAmount = returnsQuery.value("refund_amount").toDouble();
            QString reason = returnsQuery.value("reason").toString();

            bool itemMatch = lowerSearchTerm.isEmpty() ||
                itemId.toLower().contains(lowerSearchTerm) ||
                itemName.toLower().contains(lowerSearchTerm);

            if (itemMatch) {
                userHistoryRecords.emplace_back(
                    returnDate,
                    "Return",
                    itemId,
                    itemName,
                    quantity,
                    -refundAmount, // Negative for returns
                    "N/A", // Discount column for returns
                    reason
                );
            }
        }
    }
    else {
        qDebug() << "Error fetching returns for detailed history:" << returnsQuery.lastError().text();
    }


    // Sort records by date
    std::sort(userHistoryRecords.begin(), userHistoryRecords.end(), [](const auto& a, const auto& b) { //
        return std::get<0>(a) < std::get<0>(b); //
        });

    targetTable->setRowCount(userHistoryRecords.size()); //
    for (int i = 0; i < userHistoryRecords.size(); ++i) { //
        const auto& record = userHistoryRecords[i]; //
        targetTable->setItem(i, 0, new QTableWidgetItem(std::get<0>(record).toString("yyyy-MM-dd"))); //
        targetTable->setItem(i, 1, new QTableWidgetItem(std::get<1>(record))); //
        targetTable->setItem(i, 2, new QTableWidgetItem(std::get<2>(record))); //
        targetTable->setItem(i, 3, new QTableWidgetItem(std::get<3>(record))); //
        targetTable->setItem(i, 4, new QTableWidgetItem(QString::number(std::get<4>(record)))); //
        targetTable->setItem(i, 5, new QTableWidgetItem(QString("LE%1").arg(std::get<5>(record), 0, 'f', 2))); //
        targetTable->setItem(i, 6, new QTableWidgetItem(std::get<6>(record))); // NEW: Discount Status
        targetTable->setItem(i, 7, new QTableWidgetItem(std::get<7>(record))); // Original Details column, now at index 7

        // Color code rows based on type
        if (std::get<1>(record) == "Return") { //
            for (int col = 0; col < targetTable->columnCount(); ++col) { //
                targetTable->item(i, col)->setForeground(Qt::darkRed); //
            }
        }
    }
}


void MainWindow::refreshSupplierTable(const std::string& supplierName, QTableWidget* table) {
    const auto& transactions = inventory.getSupplierTransactionsForSupplier(supplierName);
    table->setRowCount(transactions.size());

    for (int i = 0; i < transactions.size(); ++i) {
        const auto& tx = transactions[i];

        // Transaction date
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(tx.date)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(tx.supplierName)));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(tx.totalAmount, 'f', 2)));
        table->setItem(i, 3, new QTableWidgetItem(QString::number(tx.paidAmount, 'f', 2)));

        // Remaining amount with color coding
        double remaining = tx.getRemainingAmount();
        QTableWidgetItem* remainingItem = new QTableWidgetItem(QString::number(remaining, 'f', 2));
        remainingItem->setForeground(remaining > 0 ? Qt::red : Qt::darkGreen);
        table->setItem(i, 4, remainingItem);

        QString status;
        if (remaining <= 0) status = "✅ Paid in full";
        else if (tx.paidAmount > 0) status = QString("⏳ %1% paid").arg((tx.paidAmount / tx.totalAmount) * 100, 0, 'f', 1);
        else status = "❌ Unpaid";
        table->setItem(i, 5, new QTableWidgetItem(status));

        // Payment Details Column: Now clickable
        QTableWidgetItem* paymentDetailsItem = new QTableWidgetItem(tx.paymentHistory.empty() ? "Click to view" : "View Payments");
        paymentDetailsItem->setTextAlignment(Qt::AlignCenter);
        paymentDetailsItem->setFlags(paymentDetailsItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 6, paymentDetailsItem);
    }
}

void MainWindow::addSupplierPayment(const std::string& supplierName, QDateEdit* paymentDateInput, QLineEdit* paymentInput, QFrame* summaryCard, QTableWidget* table) {
    bool ok;
    double amount = paymentInput->text().toDouble(&ok);
    QString date = paymentDateInput->date().toString("yyyy-MM-dd"); // Use YYYY-MM-DD for consistency

    if (ok && amount > 0) {
        inventory.addSupplierPayment(supplierName, date.toStdString(), amount);
        refreshSupplierTable(supplierName, table);

        // Update summary card
        QLabel* valueLabel = summaryCard->findChild<QLabel*>("valueLabel");
        if (valueLabel) {
            double newRemaining = inventory.getTotalRemainingForSupplier(supplierName);
            valueLabel->setText(QString("LE%1").arg(newRemaining, 0, 'f', 2));
            summaryCard->setStyleSheet(QString(
                "background-color: %1;"
                "border-radius: 8px;"
                "padding: 15px;"
                "color: white;"
            ).arg(newRemaining > 0 ? "#e74c3c" : "#2ecc71"));
        }
        paymentInput->clear();
    }
    else {
        QMessageBox::warning(this, "Input Error", "Please enter a valid payment amount.");
    }
}

void MainWindow::addSupplierTransactionFromForm() {
    QString name = supplierNameInput->text().trimmed();
    QString date = supplierDateInput->date().toString("yyyy-MM-dd"); // Use YYYY-MM-DD for consistency
    double total = totalAmountInput->text().toDouble();
    double paid = paidAmountInput->text().toDouble();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Error", "Supplier name is required");
        return;
    }

    if (paid > total) {
        QMessageBox::warning(this, "Error", "Paid amount cannot exceed total.");
        return;
    }

    inventory.addSupplierTransaction(date.toStdString(), name.toStdString(), total, paid);

    // This part refreshes only the current supplier's tab if it exists
    // A full refreshAllTabs is handled by the lambda in showSuppliersDashboard
    auto it = supplierTables.find(name.toStdString());
    if (it != supplierTables.end()) {
        refreshSupplierTable(name.toStdString(), it->second);
        if (supplierTabWidget) {
            for (int i = 0; i < supplierTabWidget->count(); ++i) {
                if (supplierTabWidget->tabText(i) == name) {
                    QWidget* tab = supplierTabWidget->widget(i);
                    QFrame* summaryCard = tab->findChild<QFrame*>("summaryCard"); // Find by objectName
                    if (summaryCard) {
                        double totalRemaining = inventory.getTotalRemainingForSupplier(name.toStdString());
                        QLabel* valueLabel = summaryCard->findChild<QLabel*>("valueLabel");
                        if (valueLabel) {
                            valueLabel->setText(QString("LE%1").arg(totalRemaining, 0, 'f', 2));
                            summaryCard->setStyleSheet(QString(
                                "background-color: %1;"
                                "border-radius: 8px;"
                                "padding: 15px;"
                                "color: white;"
                            ).arg(totalRemaining > 0 ? "#e74c3c" : "#2ecc71"));
                        }
                    }
                    break;
                }
            }
        }
    }
    else {
        // If it's a new supplier, the tab won't exist yet. The refreshAllTabs
        // in showSuppliersDashboard (connected to refresh button) needs to be triggered.
        QMessageBox::information(this, "Supplier Added", "New supplier transaction added. Click 'Refresh Dashboard' to see the new supplier's tab.");
    }

    supplierNameInput->clear();
    totalAmountInput->clear();
    paidAmountInput->clear();
    supplierDateInput->setDate(QDate::currentDate());
}


bool MainWindow::hasDiscounts(const std::vector<OrderItem>& items) {
    for (const auto& item : items) {
        if (item.discountPercent > 0) {
            return true;
        }
    }
    return false;
}

void MainWindow::updateUserDisplay() {
    QString username = QString::fromStdString(inventory.getCurrentUser());
    if (!username.isEmpty()) {
        QLabel* userLabel = findChild<QLabel*>("userLabel");
        if (userLabel) {
            userLabel->setText("Welcome, " + username);
        }
    }
}

void MainWindow::setCurrentUser(const std::string& username) {
    inventory.setCurrentUser(username);
    updateUserDisplay();
    // Reset so the low-stock popup fires once for this new login session
    lowStockNotifiedThisSession_ = false;
}


//void MainWindow::initializeInventory() {
    // This function will now ensure some default items exist in the database
    // if the database is initially empty.
    // In a real application, this might be part of a setup script or migration.

    // Check if the database has any items
    //if (inventory.getInventory().empty()) {
       // inventory.addItem("PEN001", "Blue Pens", 50, 1.50, 0.75);
       // inventory.addItem("PEN002", "Black Pens", 45, 1.50, 0.80);
       // inventory.addItem("PENCIL001", "HB Pencils", 30, 0.75, 0.30);
       //inventory.addItem("BOOK001", "Notebooks A4", 20, 15.99, 10.00);
       // inventory.addItem("BOOK002", "Notebooks A5", 25, 12.99, 8.50);
        //inventory.addItem("ERASER001", "White Erasers", 40, 0.50, 0.20);
        //inventory.addItem("RULER001", "Plastic Rulers", 35, 2.25, 1.00);
        //qDebug() << "Initialized default inventory items to database.";
    //}
    //else {
       // qDebug() << "Database already contains inventory items. Skipping default initialization.";
   // }
//}

void MainWindow::setupUI() {
    setWindowTitle("Library Inventory System");
    resize(1000, 700);
    setMinimumSize(900, 650);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(0);

    QFrame* headerFrame = new QFrame();
    headerFrame->setObjectName("headerFrame");
    headerFrame->setFixedHeight(80);

    QHBoxLayout* headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(30, 15, 30, 15);

    QLabel* logoLabel = new QLabel("📚");
    logoLabel->setStyleSheet("font-size: 32px;");

    QLabel* titleLabel = new QLabel("Library Inventory System");
    titleLabel->setObjectName("mainTitle");

    userLabel = new QLabel("Welcome, Admin");
    userLabel->setObjectName("userLabel");

    // Backup status label — shown in header after startup
    backupStatusLabel_ = new QLabel("Backup...");
    backupStatusLabel_->setStyleSheet(
        "font-size:11px; color:rgba(255,255,255,0.85);"
        "background-color:rgba(100,100,100,0.4);"
        "border-radius:10px; padding:3px 10px;");
    backupStatusLabel_->setToolTip("Checking backup status...");
    headerLayout->addWidget(backupStatusLabel_);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(userLabel);

    tabWidget = new QTabWidget();
    tabWidget->setObjectName("mainTabWidget");

    setupOrderTab();
    setupSearchTab();
    setupIndebtednessTab();
    setupReturnsTab();
    setupDeficienciesTab(); // NEW: Call to set up the deficiencies tab

    logoutButton = new QPushButton("Logout");
    logoutButton->setObjectName("dangerButton");
    logoutButton->setCursor(Qt::PointingHandCursor);
    logoutButton->setFixedSize(80, 30);

    totalHistoryButton = new QPushButton("Admin History");
    totalHistoryButton->setObjectName("adminButton");
    totalHistoryButton->setCursor(Qt::PointingHandCursor);
    totalHistoryButton->setFixedSize(120, 30);

    suppliersButton = new QPushButton("Suppliers");
    suppliersButton->setObjectName("adminButton");
    suppliersButton->setCursor(Qt::PointingHandCursor);
    suppliersButton->setFixedSize(120, 30);

    inventoryButton = new QPushButton("Inventory");
    inventoryButton->setObjectName("adminButton");
    inventoryButton->setCursor(Qt::PointingHandCursor);
    inventoryButton->setFixedSize(120, 30);

    backupButton = new QPushButton("💾 Backup");
    backupButton->setObjectName("adminButton");
    backupButton->setCursor(Qt::PointingHandCursor);
    backupButton->setFixedSize(100, 30);

    headerLayout->addWidget(totalHistoryButton);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(suppliersButton);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(inventoryButton);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(backupButton);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(logoutButton);

    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::handleLogout);
    connect(totalHistoryButton, &QPushButton::clicked, this, &MainWindow::showTotalSalesHistory);
    connect(suppliersButton, &QPushButton::clicked, this, &MainWindow::showSuppliersDashboard);
    connect(inventoryButton, &QPushButton::clicked, this, &MainWindow::showInventoryManagement);
    connect(backupButton, &QPushButton::clicked, this, &MainWindow::showBackupManager);

    // NEW: Connect tab changes to manage default buttons and deficiencies timer
    connect(tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        // Find the index of the Deficiencies tab
        int deficienciesTabIndex = -1;
        int orderTabIndex = -1;
        int returnsTabIndex = -1;
        int indebtednessTabIndex = -1;
        // Search tab's "Search" button is already handled by QLineEdit::returnPressed.

        for (int i = 0; i < tabWidget->count(); ++i) {
            if (tabWidget->tabText(i) == "⚠️ Deficiencies") {
                deficienciesTabIndex = i;
            }
            else if (tabWidget->tabText(i) == "🛒 New Order") {
                orderTabIndex = i;
            }
            else if (tabWidget->tabText(i) == "↩️ Returns") {
                returnsTabIndex = i;
            }
            else if (tabWidget->tabText(i) == "💳 Indebtedness") {
                indebtednessTabIndex = i;
            }
        }

        // Reset all relevant buttons to not be default
        if (completeOrderButton) completeOrderButton->setDefault(false);
        if (processReturnButton) processReturnButton->setDefault(false);
        if (debtSubmitButton) debtSubmitButton->setDefault(false);

        // Set the default button based on the active tab
        if (index == orderTabIndex) {
            if (completeOrderButton) completeOrderButton->setDefault(true);
        }
        else if (index == returnsTabIndex) {
            if (processReturnButton) processReturnButton->setDefault(true);
        }
        else if (index == indebtednessTabIndex) {
            if (debtSubmitButton) debtSubmitButton->setDefault(true);
        }
        // For Suppliers and Inventory dialogs, their default buttons will be set within their show functions.

        // Deficiencies tab timer management
        if (index == deficienciesTabIndex) {
            if (deficienciesRefreshTimer && !deficienciesRefreshTimer->isActive()) {
                deficienciesRefreshTimer->start();
                refreshDeficienciesTable(); // Optional: Refresh immediately when activated
            }
        }
        else {
            if (deficienciesRefreshTimer && deficienciesRefreshTimer->isActive()) {
                deficienciesRefreshTimer->stop();
            }
        }
        });

    // Ensure the correct default button is set on initial load
    tabWidget->currentChanged(tabWidget->currentIndex());


    mainLayout->addWidget(headerFrame);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(tabWidget);

    applyModernStyling();
    addShadowEffects();
}

void MainWindow::setupOrderTab() {
    QWidget* orderTab = new QWidget();
    QHBoxLayout* mainLayout = new QHBoxLayout(orderTab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    QFrame* orderFormFrame = new QFrame();
    orderFormFrame->setObjectName("cardFrame");
    orderFormFrame->setFixedWidth(320);
    orderFormFrame->setMaximumHeight(520);

    QVBoxLayout* formLayout = new QVBoxLayout(orderFormFrame);
    formLayout->setContentsMargins(25, 25, 25, 25);
    formLayout->setSpacing(20);

    QLabel* formTitle = new QLabel("Add to Order");
    formTitle->setObjectName("sectionTitle");

    QVBoxLayout* orderFieldsLayout = new QVBoxLayout();
    orderFieldsLayout->setSpacing(15);

    QVBoxLayout* orderIdLayout = new QVBoxLayout();
    orderIdLayout->setSpacing(5);
    QLabel* orderIdLabel = new QLabel("Item ID / Name Search");
    orderIdLabel->setObjectName("fieldLabel");
    orderItemIdInput = new QLineEdit();
    orderItemIdInput->setObjectName("styledInput");
    orderItemIdInput->setPlaceholderText("Scan barcode or type item name...");
    orderIdLayout->addWidget(orderIdLabel);
    orderIdLayout->addWidget(orderItemIdInput);

    // Name-search dropdown list (hidden until user types a name)
    orderItemSuggestionsList = new QListWidget();
    orderItemSuggestionsList->setObjectName("orderSuggestionsList");
    orderItemSuggestionsList->setMaximumHeight(180);
    orderItemSuggestionsList->hide();
    orderItemSuggestionsList->setStyleSheet(R"(
        QListWidget#orderSuggestionsList {
            border: 2px solid #667eea;
            border-radius: 8px;
            background-color: white;
            font-size: 13px;
            padding: 4px;
        }
        QListWidget#orderSuggestionsList::item {
            padding: 8px 10px;
            border-bottom: 1px solid #f0f0f0;
            color: #2c3e50;
        }
        QListWidget#orderSuggestionsList::item:last {
            border-bottom: none;
        }
        QListWidget#orderSuggestionsList::item:hover {
            background-color: #eef1ff;
            color: #667eea;
        }
        QListWidget#orderSuggestionsList::item:selected {
            background-color: #667eea;
            color: white;
        }
    )");
    orderIdLayout->addWidget(orderItemSuggestionsList);

    // textChanged: search by ID or name and populate dropdown
    connect(orderItemIdInput, &QLineEdit::textChanged, this, [this](const QString& text) {
        orderItemSuggestionsList->clear();
        if (text.isEmpty()) {
            orderItemSuggestionsList->hide();
            return;
        }

        // If text is purely numeric (barcode scan in progress), skip name search
        bool isNumeric = true;
        for (const QChar& ch : text) {
            if (!ch.isDigit()) { isNumeric = false; break; }
        }
        if (isNumeric) {
            orderItemSuggestionsList->hide();
            return;
        }

        // Search inventory by name (and ID fallback)
        const auto& inv = inventory.getInventory();
        QString lower = text.toLower();
        int count = 0;
        for (const auto& pair : inv) {
            const Item& item = pair.second;
            QString itemName = QString::fromStdString(item.name);
            QString itemId = QString::fromStdString(item.id);
            if (itemName.toLower().contains(lower) || itemId.toLower().contains(lower)) {
                // Format: "Name  |  ID  |  Qty: X  |  LE price"
                QString display = QString("%1   [%2]   Qty: %3   LE %4")
                    .arg(itemName)
                    .arg(itemId)
                    .arg(item.quantity)
                    .arg(item.price, 0, 'f', 2);
                QListWidgetItem* listItem = new QListWidgetItem(display);
                // Store the actual item_id as user data so we can retrieve it on click
                listItem->setData(Qt::UserRole, itemId);
                if (item.quantity == 0) {
                    listItem->setForeground(QColor("#adb5bd")); // grey out out-of-stock
                    listItem->setToolTip("Out of stock");
                }
                orderItemSuggestionsList->addItem(listItem);
                if (++count >= 8) break; // cap at 8 results
            }
        }

        if (orderItemSuggestionsList->count() > 0) {
            orderItemSuggestionsList->show();
        }
        else {
            orderItemSuggestionsList->hide();
        }
        });

    // Clicking a suggestion fills the ID field and moves to quantity
    connect(orderItemSuggestionsList, &QListWidget::itemClicked, this,
        [this](QListWidgetItem* listItem) {
            QString selectedId = listItem->data(Qt::UserRole).toString();
            orderItemIdInput->setText(selectedId);
            orderItemSuggestionsList->hide();
            orderItemSuggestionsList->clear();
            orderQuantityInput->setFocus();
            orderQuantityInput->selectAll();
        });

    // Auto-tabbing for orderItemIdInput (barcode scan Enter)
    connect(orderItemIdInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = orderItemIdInput->text().trimmed();
        if (text.isEmpty()) return;

        // If suggestion list is visible and has items, pick the first one
        if (orderItemSuggestionsList->isVisible() && orderItemSuggestionsList->count() > 0) {
            QListWidgetItem* first = orderItemSuggestionsList->item(0);
            QString selectedId = first->data(Qt::UserRole).toString();
            orderItemIdInput->setText(selectedId);
            orderItemSuggestionsList->hide();
            orderItemSuggestionsList->clear();
            orderQuantityInput->setFocus();
            orderQuantityInput->selectAll();
            return;
        }

        // Normal barcode path: check DB and move to quantity
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM items WHERE item_id = :item_id");
        query.bindValue(":item_id", text);
        if (query.exec() && query.next() && query.value(0).toInt() > 0) {
            orderQuantityInput->setFocus();
        }
        else {
            orderItemIdInput->selectAll();
        }
        });

    QVBoxLayout* orderQuantityLayout = new QVBoxLayout();
    orderQuantityLayout->setSpacing(5);
    QLabel* orderQuantityLabel = new QLabel("Quantity");
    orderQuantityLabel->setObjectName("fieldLabel");
    orderQuantityInput = new QLineEdit();
    orderQuantityInput->setObjectName("styledInput");
    orderQuantityInput->setPlaceholderText("Enter quantity");
    orderQuantityLayout->addWidget(orderQuantityLabel);
    orderQuantityLayout->addWidget(orderQuantityInput);

    orderFieldsLayout->addLayout(orderIdLayout);
    orderFieldsLayout->addLayout(orderQuantityLayout);

    QVBoxLayout* discountLayout = new QVBoxLayout();
    discountLayout->setSpacing(5);
    QLabel* discountLabel = new QLabel("Discount (%)");
    discountLabel->setObjectName("fieldLabel");
    orderDiscountInput = new QLineEdit();
    orderDiscountInput->setObjectName("styledInput");
    orderDiscountInput->setPlaceholderText("Optional (e.g., 10)");
    discountLayout->addWidget(discountLabel);
    discountLayout->addWidget(orderDiscountInput);

    orderFieldsLayout->addLayout(discountLayout);

    addToOrderButton = new QPushButton("Add to Order");
    addToOrderButton->setObjectName("secondaryButton");
    addToOrderButton->setMinimumHeight(40);
    addToOrderButton->setCursor(Qt::PointingHandCursor);

    formLayout->addWidget(formTitle);
    formLayout->addLayout(orderFieldsLayout);
    formLayout->addWidget(addToOrderButton);
    formLayout->addStretch();

    QFrame* orderTableFrame = new QFrame();
    orderTableFrame->setObjectName("cardFrame");

    QVBoxLayout* tableLayout = new QVBoxLayout(orderTableFrame);
    tableLayout->setContentsMargins(25, 25, 25, 25);
    tableLayout->setSpacing(15);

    QLabel* orderTableTitle = new QLabel("Current Order");
    orderTableTitle->setObjectName("sectionTitle");

    orderTable = new QTableWidget();
    orderTable->setObjectName("styledTable");
    orderTable->setColumnCount(6);
    orderTable->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Quantity", "Unit Price", "Discount (%)", "Total Price" });
    orderTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    orderTable->setAlternatingRowColors(true);
    orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    orderTable->verticalHeader()->setVisible(false);

    QFrame* actionsFrame = new QFrame();
    actionsFrame->setObjectName("actionsFrame");
    actionsFrame->setMinimumHeight(80);

    QHBoxLayout* actionsLayout = new QHBoxLayout(actionsFrame);
    actionsLayout->setContentsMargins(20, 15, 20, 15);
    actionsLayout->setSpacing(15);

    completeOrderButton = new QPushButton("Complete Order");
    completeOrderButton->setObjectName("successButton");
    completeOrderButton->setMinimumHeight(45);
    completeOrderButton->setMinimumWidth(140);
    completeOrderButton->setCursor(Qt::PointingHandCursor);
    completeOrderButton->setDefault(true); // Set as default for this tab

    cancelOrderButton = new QPushButton("Cancel Order");
    cancelOrderButton->setObjectName("dangerButton");
    cancelOrderButton->setMinimumHeight(45);
    cancelOrderButton->setMinimumWidth(120);
    cancelOrderButton->setCursor(Qt::PointingHandCursor);

    // New: Deorder Button
    deorderButton = new QPushButton("Deorder Selected Item");
    deorderButton->setObjectName("secondaryButton"); // Or 'warningButton' if you have one, to differentiate
    deorderButton->setMinimumHeight(45);
    deorderButton->setMinimumWidth(160);
    deorderButton->setCursor(Qt::PointingHandCursor);

    // NEW: Print Receipt Button
    printReceiptButton = new QPushButton("Print Last Receipt");
    printReceiptButton->setObjectName("primaryButton");
    printReceiptButton->setMinimumHeight(45);
    printReceiptButton->setMinimumWidth(140);
    printReceiptButton->setCursor(Qt::PointingHandCursor);
    printReceiptButton->setEnabled(false); // Disabled initially until an order is completed


    orderTotalLabel = new QLabel("Total: LE0.00");
    orderTotalLabel->setObjectName("totalLabel");

    actionsLayout->addWidget(completeOrderButton);
    actionsLayout->addWidget(cancelOrderButton);
    actionsLayout->addWidget(deorderButton); // Add the new button
    actionsLayout->addWidget(printReceiptButton); // NEW: Add print receipt button
    actionsLayout->addStretch();
    actionsLayout->addWidget(orderTotalLabel);

    tableLayout->addWidget(orderTableTitle);
    tableLayout->addWidget(orderTable);
    tableLayout->addWidget(actionsFrame);

    mainLayout->addWidget(orderFormFrame);
    mainLayout->addWidget(orderTableFrame);

    // Enter on Quantity --> move focus to Discount
    connect(orderQuantityInput, &QLineEdit::returnPressed, this, [this]() {
        if (!orderQuantityInput->text().trimmed().isEmpty())
            orderDiscountInput->setFocus();
        });

    // Enter on Discount (or when discount is empty and user hits Enter) --> Add to Order
    connect(orderDiscountInput, &QLineEdit::returnPressed, this, &MainWindow::addItemToOrder);

    connect(addToOrderButton, &QPushButton::clicked, this, &MainWindow::addItemToOrder);
    connect(completeOrderButton, &QPushButton::clicked, this, &MainWindow::completeCurrentOrder);
    connect(cancelOrderButton, &QPushButton::clicked, this, &MainWindow::cancelCurrentOrder);
    connect(deorderButton, &QPushButton::clicked, this, &MainWindow::onDeorderItemClicked); // Connect new button
    connect(printReceiptButton, &QPushButton::clicked, this, [this]() {
        // Get the last sale number from the database
        QSqlQuery query;
        query.prepare("SELECT MAX(sale_number) FROM sales WHERE username = :username");
        query.bindValue(":username", QString::fromStdString(inventory.getCurrentUser()));
        if (query.exec() && query.next()) {
            int lastSaleNumber = query.value(0).toInt();
            if (lastSaleNumber > 0) {
                printReceipt(lastSaleNumber);
            }
        }
        });


    tabWidget->addTab(orderTab, "🛒 New Order");
}

void MainWindow::setupSearchTab() {
    QWidget* searchTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(searchTab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(20);

    QFrame* searchFrame = new QFrame();
    searchFrame->setObjectName("cardFrame");
    // Removed fixed height, will adjust to content including suggestion list
    // searchFrame->setMaximumHeight(120);

    QVBoxLayout* searchLayout = new QVBoxLayout(searchFrame);
    searchLayout->setContentsMargins(25, 25, 25, 25); // Fixed typo: setContentsContents -> setContentsMargins
    searchLayout->setSpacing(15);

    QLabel* searchTitle = new QLabel("Search Inventory");
    searchTitle->setObjectName("sectionTitle");

    QHBoxLayout* searchInputLayout = new QHBoxLayout();
    searchInputLayout->setSpacing(15);

    searchInput = new QLineEdit();
    searchInput->setObjectName("styledInput");
    searchInput->setPlaceholderText("Enter item ID or name to search...");
    // FIX: Connect the scanner's "Enter" key to the search function
    connect(searchInput, &QLineEdit::returnPressed, this, &MainWindow::searchItem);
    searchInput->setMinimumHeight(40);

    searchButton = new QPushButton("Search");
    searchButton->setObjectName("primaryButton");
    searchButton->setMinimumHeight(40);
    searchButton->setMinimumWidth(100);
    searchButton->setCursor(Qt::PointingHandCursor);

    searchInputLayout->addWidget(searchInput);
    searchInputLayout->addWidget(searchButton);

    searchSuggestionsList = new QListWidget(); // Initialize the list widget
    searchSuggestionsList->setObjectName("searchSuggestionsList");
    searchSuggestionsList->setMaximumHeight(150); // Set a reasonable maximum height
    searchSuggestionsList->hide(); // Initially hidden

    // Apply some styling to the suggestion list
    searchSuggestionsList->setStyleSheet(R"(
        QListWidget#searchSuggestionsList {
            border: 1px solid #ced4da;
            border-radius: 8px;
            background-color: white;
            selection-background-color: #e3f2fd;
            selection-color: #1976d2;
            padding: 5px;
        }
        QListWidget#searchSuggestionsList::item {
            padding: 8px;
            border-bottom: 1px solid #f0f0f0;
        }
        QListWidget#searchSuggestionsList::item:last {
            border-bottom: none;
        }
    )");


    searchLayout->addWidget(searchTitle);
    searchLayout->addLayout(searchInputLayout);
    searchLayout->addWidget(searchSuggestionsList); // Add the suggestion list to the layout

    QFrame* resultsFrame = new QFrame();
    resultsFrame->setObjectName("cardFrame");

    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsFrame);
    resultsLayout->setContentsMargins(25, 25, 25, 25);
    resultsLayout->setSpacing(15);

    QLabel* resultsTitle = new QLabel("Search Results");
    resultsTitle->setObjectName("sectionTitle");

    searchResultsTable = new QTableWidget();
    searchResultsTable->setObjectName("styledTable");
    searchResultsTable->setColumnCount(6);
    searchResultsTable->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Quantity", "Sell Price", "Bought Price", "Total Sell Value" });
    searchResultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    searchResultsTable->setAlternatingRowColors(true);
    searchResultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    searchResultsTable->verticalHeader()->setVisible(false);

    resultsLayout->addWidget(resultsTitle);
    resultsLayout->addWidget(searchResultsTable);

    layout->addWidget(searchFrame);
    layout->addWidget(resultsFrame);

    // Connect new signals/slots for auto-completion
    connect(searchInput, &QLineEdit::textChanged, this, &MainWindow::onSearchInputTextChanged);
    connect(searchSuggestionsList, &QListWidget::itemClicked, this, &MainWindow::onSuggestionItemSelected);
    // Keep existing search button and Enter key connections
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::searchItem);
    connect(searchInput, &QLineEdit::returnPressed, this, &MainWindow::searchItem);

    tabWidget->addTab(searchTab, "🔍 Search Items");
}

void MainWindow::setupDeficienciesTab() {
    QWidget* deficienciesTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(deficienciesTab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    QFrame* tableFrame = new QFrame();
    tableFrame->setObjectName("cardFrame");

    QVBoxLayout* tableLayout = new QVBoxLayout(tableFrame);
    tableLayout->setContentsMargins(25, 25, 25, 25);
    tableLayout->setSpacing(15);

    QLabel* tableTitle = new QLabel("Inventory Deficiencies (below per-item threshold)");
    tableTitle->setObjectName("sectionTitle");

    deficienciesTable = new QTableWidget(); // Initialize the member variable
    deficienciesTable->setObjectName("styledTable");
    deficienciesTable->setColumnCount(4); // Item ID, Item Name, Current Qty, Threshold
    deficienciesTable->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Current Qty", "Threshold" });
    deficienciesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    deficienciesTable->setAlternatingRowColors(true);
    deficienciesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    deficienciesTable->verticalHeader()->setVisible(false);

    // Keep the manual refresh button if desired, or remove it for full automation
    QPushButton* refreshButton = new QPushButton("Refresh Deficiencies");
    refreshButton->setObjectName("secondaryButton");
    refreshButton->setMinimumHeight(40);
    refreshButton->setCursor(Qt::PointingHandCursor);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshDeficienciesTable);


    tableLayout->addWidget(tableTitle);
    tableLayout->addWidget(refreshButton, 0, Qt::AlignRight); // Place button at top right
    tableLayout->addWidget(deficienciesTable);

    mainLayout->addWidget(tableFrame);

    tabWidget->addTab(deficienciesTab, "⚠️ Deficiencies"); // Add the new tab

    // NEW: Setup QTimer for auto-refresh
    deficienciesRefreshTimer = new QTimer(this);
    // Set interval (e.g., 5000 milliseconds = 5 seconds)
    deficienciesRefreshTimer->setInterval(5000); // Refresh every 5 seconds
    connect(deficienciesRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshDeficienciesTable);
    // Timer is started/stopped by currentChanged signal handler in setupUI,
    // so no explicit start() here, but a first refresh is good.
    refreshDeficienciesTable(); // Initial population of the table when tab is setup
}

void MainWindow::applyModernStyling() {
    QString styleSheet = R"(
        QMainWindow {
            background-color: #f8f9fa;
            font-family: 'Segoe UI', 'SF Pro Display', Arial, sans-serif;
        }
        #headerFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #667eea, stop:1 #764ba2);
            border-radius: 12px;
            color: white;
        }
        #mainTitle {
            font-size: 24px;
            font-weight: bold;
            color: white;
            margin-left: 15px;
        }
        #userLabel {
            font-size: 14px;
            color: rgba(255, 255, 255, 0.9);
            background-color: rgba(255, 255, 255, 0.1);
            padding: 8px 16px;
            border-radius: 20px;
        }
        #mainTabWidget {
            background-color: transparent;
        }
        #mainTabWidget::pane {
            border: none;
            background-color: transparent;
        }
        #mainTabWidget::tab-bar {
            alignment: left;
        }
        #mainTabWidget QTabBar::tab {
            background: white;
            color: #495057;
            padding: 12px 20px;
            margin-right: 4px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            font-weight: 600;
            font-size: 14px;
            min-width: 140px;
        }
        #mainTabWidget QTabBar::tab:selected {
            background: #007bff;
            color: white;
        }
        #mainTabWidget QTabBar::tab:hover:!selected {
            background: #e9ecef;
        }
        #cardFrame {
            background-color: white;
            border-radius: 12px;
            border: 1px solid #e9ecef;
        }
        #sectionTitle {
            font-size: 18px;
            font-weight: bold;
            color: #2c3e50;
            margin-bottom: 5px;
        }
        #fieldLabel {
            font-size: 14px;
            font-weight: 600;
            color: #495057;
        }
        #styledInput {
            border: 2px solid #e9ecef;
            border-radius: 8px;
            padding: 12px 16px;
            font-size: 14px;
            background-color: #fff;
            color: #495057;
            min-height: 20px;
        }
        #styledInput:focus {
            border-color: #007bff;
            outline: none;
            background-color: #fff;
        }
        #styledInput:hover {
            border-color: #ced4da;
        }
        #primaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #007bff, stop:1 #0056b3);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            padding: 8px 16px;
        }
        #primaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0056b3, stop:1 #004085);
        }
        #primaryButton:pressed {
            background: #004085;
        }
        #secondaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #6c757d, stop:1 #545b62);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            padding: 8px 16px;
        }
        #secondaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #545b62, stop:1 #3d4142);
        }
        #successButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #28a745, stop:1 #1e7e34);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            padding: 8px 16px;
        }
        #successButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1e7e34, stop:1 #155724);
        }
        #dangerButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #dc3545, stop:1 #c82333);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            padding: 8px 16px;
        }
        #dangerButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #c82333, stop:1 #bd2130);
        }
        #styledTable {
            background-color: white;
            border: 1px solid #e9ecef;
            border-radius: 8px;
            gridline-color: #e9ecef;
        }
        #styledTable::item {
            padding: 8px;
            border-bottom: 1px solid #f8f9fa;
        }
        #styledTable::item:selected {
            background-color: #e3f2fd;
            color: #1976d2;
        }
        #styledTable QHeaderView::section {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f8f9fa, stop:1 #e9ecef);
            color: #495057;
            padding: 12px 8px;
            border: none;
            border-right: 1px solid #e9ecef;
            font-weight: 600;
            font-size: 13px;
        }
        #styledTable QHeaderView::section:first {
            border-top-left-radius: 8px;
        }
        #styledTable QHeaderView::section:last {
            border-top-right-radius: 8px;
            border-right: none;
        }
        #actionsFrame {
            background-color: #f8f9fa;
            border-radius: 8px;
            border: 1px solid #e9ecef;
        }
        #totalLabel {
            font-size: 18px;
            font-weight: bold;
            color: #28a745;
            background-color: white;
            padding: 12px 20px;
            border-radius: 8px;
            border: 2px solid #28a745;
        }
        #adminButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #6f42c1, stop:1 #5a32a3);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            padding: 8px 16px;
        }
        #adminButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5a32a3, stop:1 #4a2580);
        }
        #adminButton:pressed {
            background: #4a2580;
        }
    )";
    setStyleSheet(styleSheet);
}

void MainWindow::addShadowEffects() {
    QList<QFrame*> frames = findChildren<QFrame*>();
    for (QFrame* frame : frames) {
        if (frame->objectName() == "cardFrame" || frame->objectName() == "headerFrame") {
            QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
            shadowEffect->setBlurRadius(15);
            shadowEffect->setXOffset(0);
            shadowEffect->setYOffset(3);
            shadowEffect->setColor(QColor(0, 0, 0, 30));
            frame->setGraphicsEffect(shadowEffect);
        }
    }
}

void MainWindow::clearInventoryForm() {
    itemIdInput->clear();
    itemNameInput->clear();
    itemQuantityInput->clear();
    itemPriceInput->clear();
    boughtPriceInput->clear();
    itemThresholdInput->clear();
}

void MainWindow::refreshOrderTable() {
    const auto& order = inventory.getCurrentOrder(); // In-memory current order
    orderTable->setRowCount(order.size());

    int row = 0;
    for (const auto& item : order) { // Use the local 'order' variable
        orderTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.itemId)));
        orderTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(item.itemName)));
        orderTable->setItem(row, 2, new QTableWidgetItem(QString::number(item.quantity)));
        orderTable->setItem(row, 3, new QTableWidgetItem(QString("LE%1").arg(item.unitPrice, 0, 'f', 2)));
        orderTable->setItem(row, 4, new QTableWidgetItem(QString::number(item.discountPercent, 'f', 1)));
        orderTable->setItem(row, 5, new QTableWidgetItem(QString("LE%1").arg(item.totalPrice, 0, 'f', 2)));
        ++row;
    }
    updateOrderTotal();
}

void MainWindow::updateOrderTotal() {
    double total = 0.0;
    for (const auto& item : inventory.getCurrentOrder()) {
        total += item.totalPrice;
    }
    orderTotalLabel->setText(QString("Total: LE%1").arg(total, 0, 'f', 2));
}

void MainWindow::addItemToInventory() {
    // This function is now handled by a lambda within showInventoryManagement
    // and is no longer directly connected via its own slot.
}

void MainWindow::addItemToOrder() {
    QString itemId = orderItemIdInput->text().trimmed();
    int quantity = orderQuantityInput->text().toInt();

    double discount = 0.0;
    if (!orderDiscountInput->text().trimmed().isEmpty()) {
        discount = orderDiscountInput->text().toDouble();
        if (discount < 0.0 || discount > 100.0) {
            QMessageBox::warning(this, "Invalid Discount", "Discount must be between 0 and 100.");
            return;
        }
    }

    if (itemId.isEmpty() || quantity <= 0) {
        QMessageBox::warning(this, "Input Error", "Please enter a valid Item ID and quantity.");
        return;
    }

    // Pass the discount percentage to addToOrder
    if (inventory.addToOrder(itemId.toStdString(), quantity, discount)) {
        refreshOrderTable(); // Refresh the in-memory order table
        updateOrderTotal();
        clearOrderForm();
        // Also refresh the search results table and deficiencies table to reflect inventory changes
        searchItem(); // Refresh search results (shows all if no search term)
        refreshDeficienciesTable(); // Refresh deficiencies
    }
    else {
        QMessageBox::warning(this, "Order Error", "Item not found or insufficient quantity in inventory.");
    }
}

void MainWindow::onDeorderItemClicked() {
    QList<QTableWidgetItem*> selectedItems = orderTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Selection Error", "Please select an item from the order to deorder.");
        return;
    }

    // Get the item ID from the first column of the selected row
    // Assuming Item ID is in column 0
    QString itemId = orderTable->item(selectedItems.first()->row(), 0)->text();

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm Deorder",
        QString("Are you sure you want to remove '%1' from the current order?").arg(itemId),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (inventory.removeItemFromOrder(itemId.toStdString())) {
            QMessageBox::information(this, "Deorder Successful",
                QString("Item '%1' has been removed from the order and quantity returned to inventory.").arg(itemId));
            refreshOrderTable(); // Refresh the order table
            searchItem(); // Refresh search results (shows all if no search term)
            refreshDeficienciesTable(); // Refresh deficiencies
        }
        else {
            QMessageBox::warning(this, "Deorder Error", "Failed to remove item from order. Item not found in order.");
        }
    }
}


void MainWindow::completeCurrentOrder() {
    if (inventory.getCurrentOrder().empty()) {
        QMessageBox::warning(this, "Error", "No items in current order");
        return;
    }

    if (inventory.getCurrentUser().empty()) { // Check if a user is logged in
        QMessageBox::warning(this, "Error", "Cannot complete order: No user is logged in.");
        return;
    }

    if (inventory.completeOrder()) {
        // Get the sale number of the just-completed order
        QSqlQuery query;
        query.prepare("SELECT MAX(sale_number) FROM sales WHERE username = :username");
        query.bindValue(":username", QString::fromStdString(inventory.getCurrentUser()));
        int lastSaleNumber = 0;
        if (query.exec() && query.next()) {
            lastSaleNumber = query.value(0).toInt();
        }

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Order Completed",
            QString("Order completed successfully!\nTotal: %1\n\nWould you like to print the receipt?")
            .arg(orderTotalLabel->text()),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes && lastSaleNumber > 0) {
            printReceipt(lastSaleNumber);
        }

        // Enable the print receipt button for later printing if needed
        printReceiptButton->setEnabled(true);

        refreshOrderTable();
        searchItem();
        refreshDeficienciesTable();
    }
    else {
        QMessageBox::warning(this, "Order Error", "Failed to complete order. No user logged in or other issue.");
    }
}

void MainWindow::cancelCurrentOrder() {
    inventory.cancelOrder();
    refreshOrderTable(); // Clears the current order display
    searchItem(); // Refresh search results (shows all if no search term)
    refreshDeficienciesTable(); // Refresh deficiencies
    QMessageBox::information(this, "Order Cancelled", "Current order has been cancelled");
}


void MainWindow::printReceipt(int saleNumber) {
    // 1. Fetch sale header
    QSqlQuery saleQuery;
    saleQuery.prepare("SELECT sale_date, total_amount, username FROM sales WHERE sale_number = :id");
    saleQuery.bindValue(":id", saleNumber);
    if (!saleQuery.exec() || !saleQuery.next()) {
        QMessageBox::warning(this, "Print Error", "Could not find sale details.");
        return;
    }
    QString saleDate = saleQuery.value("sale_date").toString();
    double  totalAmount = saleQuery.value("total_amount").toDouble();
    QString username = saleQuery.value("username").toString();

    // 2. Fetch sale items
    QSqlQuery itemsQuery;
    itemsQuery.prepare("SELECT item_name, quantity, unit_price, discount_percent, total_price "
        "FROM sale_items WHERE sale_number = :id");
    itemsQuery.bindValue(":id", saleNumber);
    itemsQuery.exec();

    // 3. Build HTML item rows
    QString itemRows;
    while (itemsQuery.next()) {
        QString name = itemsQuery.value("item_name").toString().toHtmlEscaped();
        int     qty = itemsQuery.value("quantity").toInt();
        double  price = itemsQuery.value("unit_price").toDouble();
        double  discount = itemsQuery.value("discount_percent").toDouble();
        double  total = itemsQuery.value("total_price").toDouble();
        QString disc = (discount > 0) ? QString("%1%").arg(discount, 0, 'f', 1) : QString("-");

        itemRows += QString(
            "<tr>"
            "<td style='padding:6px 8px;border-bottom:1px solid #ddd;'>%1</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #ddd;text-align:center;'>%2</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #ddd;text-align:right;'>LE %3</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #ddd;text-align:center;'>%4</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #ddd;text-align:right;'>LE %5</td>"
            "</tr>"
        ).arg(name).arg(qty)
            .arg(price, 0, 'f', 2).arg(disc)
            .arg(total, 0, 'f', 2);
    }

    // 4. Build full HTML receipt
    QString html = QString(
        "<html>"
        "<body style='font-family:Arial,sans-serif; font-size:11pt; margin:0; padding:0;'>"
        "<h2 style='text-align:center; font-size:16pt; letter-spacing:3px; margin-bottom:4pt; margin-top:0; direction:rtl;'>مكتبه الشيخ</h2>"
        "<hr style='border:1px solid #333;'/>"
        "<table width='100%' cellspacing='0' cellpadding='3' style='font-size:10pt;'>"
        "  <tr><td><b>Sale #:</b> %1</td><td align='right'><b>Date:</b> %2</td></tr>"
        "  <tr><td colspan='2'><b>Cashier:</b> %3</td></tr>"
        "</table>"
        "<hr style='border:0.5px solid #aaa;'/>"
        "<table width='100%' cellspacing='0' cellpadding='0' style='border-collapse:collapse; font-size:10pt;'>"
        "  <thead>"
        "    <tr style='background-color:#f0f0f0;'>"
        "      <th style='padding:6px 8px; text-align:left;   border-bottom:2px solid #555;'>Item</th>"
        "      <th style='padding:6px 8px; text-align:center; border-bottom:2px solid #555;'>Qty</th>"
        "      <th style='padding:6px 8px; text-align:right;  border-bottom:2px solid #555;'>Unit Price</th>"
        "      <th style='padding:6px 8px; text-align:center; border-bottom:2px solid #555;'>Discount</th>"
        "      <th style='padding:6px 8px; text-align:right;  border-bottom:2px solid #555;'>Total</th>"
        "    </tr>"
        "  </thead>"
        "  <tbody>%4</tbody>"
        "</table>"
        "<hr style='border:1px solid #333; margin-top:8pt;'/>"
        "<table width='100%' cellspacing='0' cellpadding='4'>"
        "  <tr><td align='right' style='font-size:13pt; font-weight:bold;'>TOTAL: LE %5</td></tr>"
        "</table>"
        "<br/>"
        "<p style='text-align:center; font-size:9pt; color:#777;'>Thank you for your purchase!</p>"
        "</body></html>"
    )
        .arg(saleNumber)
        .arg(saleDate.toHtmlEscaped())
        .arg(username.toHtmlEscaped())
        .arg(itemRows)
        .arg(totalAmount, 0, 'f', 2);

    // 5. Setup printer
    QPrinter printer(QPrinter::ScreenResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QPrintDialog printDialog(&printer, this);
    printDialog.setWindowTitle("Print Receipt");
    if (printDialog.exec() == QDialog::Rejected) {
        return;
    }

    // 6. Render HTML via QTextDocument
    QTextDocument doc;
    doc.setHtml(html);
    QSizeF pageSize = printer.pageLayout().paintRect(QPageLayout::Point).size();
    doc.setPageSize(pageSize);
    doc.print(&printer);
}

// New: Slot for handling text changes in the search input
void MainWindow::onSearchInputTextChanged(const QString& text) {
    searchSuggestionsList->clear();
    if (text.isEmpty()) {
        searchSuggestionsList->hide();
        return;
    }

    const auto& inv = inventory.getInventory(); // Get inventory from DB
    QString lowerCaseText = text.toLower();

    for (const auto& pair : inv) {
        const Item& item = pair.second;
        // Check if item name or ID contains the search text (case-insensitive)
        if (QString::fromStdString(item.name).toLower().contains(lowerCaseText) ||
            QString::fromStdString(item.id).toLower().contains(lowerCaseText)) {
            // Display both ID and Name in the suggestion list
            searchSuggestionsList->addItem(QString::fromStdString(item.id) + " - " + QString::fromStdString(item.name));
        }
    }

    if (searchSuggestionsList->count() > 0) {
        searchSuggestionsList->show();
    }
    else {
        searchSuggestionsList->hide();
    }
}

// New: Slot for handling selection of an item from the suggestion list
void MainWindow::onSuggestionItemSelected(QListWidgetItem* item) {
    if (item) {
        // Extract the full item name/ID from the selected text
        QString selectedText = item->text();
        // Assuming format "ID - Name", get the ID or full string for search
        QString itemIdOrName = selectedText.split(" - ").first().trimmed(); // Get ID part
        searchInput->setText(itemIdOrName); // Set search input to the selected ID

        searchSuggestionsList->hide(); // Hide the suggestions
        searchSuggestionsList->clear(); // Clear the suggestions
        searchItem(); // Perform a full search for the selected item
    }
}


void MainWindow::handleLogout() {
    emit logoutRequested();
}

void MainWindow::clearOrderForm() {
    orderItemIdInput->clear();
    orderQuantityInput->clear();
    orderDiscountInput->clear();
}

void MainWindow::addSupplierTransactionDialog() {
    // This dialog is likely now redundant or needs re-evaluation
    // since addSupplierTransactionFromForm handles the direct input.
    // If you intend to keep a separate dialog, it needs to be updated
    // to call the new database-backed addSupplierTransaction.
    AddSupplierDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getSupplierName();
        QString date = dialog.getDate();
        double total = dialog.getTotalAmount();
        double paid = dialog.getPaidAmount();

        if (!name.isEmpty()) {
            inventory.addSupplierTransaction(date.toStdString(), name.toStdString(), total, paid);
        }
        else {
            QMessageBox::warning(this, "Input Error", "Supplier name cannot be empty.");
        }
    }
}

// MODIFIED: Setup Indebtedness Tab
void MainWindow::setupIndebtednessTab() {
    QWidget* indebtedTab = new QWidget();
    QHBoxLayout* mainLayout = new QHBoxLayout(indebtedTab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    QFrame* formFrame = new QFrame();
    formFrame->setObjectName("cardFrame");
    formFrame->setFixedWidth(320);

    QVBoxLayout* formLayout = new QVBoxLayout(formFrame);
    formLayout->setContentsMargins(25, 25, 25, 25);
    formLayout->setSpacing(20);

    QLabel* formTitle = new QLabel("Add Indebtedness");
    formTitle->setObjectName("sectionTitle");

    debtCustomerInput = new QLineEdit();
    debtCustomerInput->setObjectName("styledInput");
    debtCustomerInput->setPlaceholderText("Customer name");

    debtDateInput = new QDateEdit(QDate::currentDate());
    debtDateInput->setCalendarPopup(true);
    debtDateInput->setDisplayFormat("yyyy-MM-dd"); // Use YYYY-MM-DD for consistency
    debtDateInput->setObjectName("styledInput");

    debtAmountInput = new QLineEdit();
    debtAmountInput->setObjectName("styledInput");
    debtAmountInput->setPlaceholderText("Amount owed");
    debtAmountInput->setValidator(new QDoubleValidator(0.0, 9999999.99, 2, this));


    debtGoodsInput = new QLineEdit();
    debtGoodsInput->setObjectName("styledInput");
    debtGoodsInput->setPlaceholderText("Goods bought");

    debtSubmitButton = new QPushButton("Add Record");
    debtSubmitButton->setObjectName("secondaryButton");
    debtSubmitButton->setMinimumHeight(40);
    debtSubmitButton->setCursor(Qt::PointingHandCursor);
    debtSubmitButton->setDefault(true); // Set as default for this tab

    formLayout->addWidget(formTitle);
    formLayout->addWidget(debtCustomerInput);
    formLayout->addWidget(debtDateInput);
    formLayout->addWidget(debtAmountInput);
    formLayout->addWidget(debtGoodsInput);
    formLayout->addWidget(debtSubmitButton);
    formLayout->addStretch();

    QFrame* tabsFrame = new QFrame();
    tabsFrame->setObjectName("cardFrame");

    QVBoxLayout* tabLayout = new QVBoxLayout(tabsFrame);
    tabLayout->setContentsMargins(25, 25, 25, 25);
    tabLayout->setSpacing(15);

    QPushButton* refreshIndebtednessButton = new QPushButton("🔄 Refresh Customers", tabsFrame);
    refreshIndebtednessButton->setObjectName("secondaryButton");
    tabLayout->addWidget(refreshIndebtednessButton, 0, Qt::AlignRight);


    QLabel* tabsTitle = new QLabel("Customer Indebtedness", tabsFrame);
    tabsTitle->setObjectName("sectionTitle");
    tabLayout->addWidget(tabsTitle);

    indebtednessTab = new QTabWidget();
    tabLayout->addWidget(indebtednessTab);

    mainLayout->addWidget(formFrame);
    mainLayout->addWidget(tabsFrame);

    // Fixed: Linker error by re-adding the implementation of handleAddIndebtedness
    connect(debtSubmitButton, &QPushButton::clicked, this, &MainWindow::handleAddIndebtedness);
    connect(refreshIndebtednessButton, &QPushButton::clicked, this, [this]() {
        // Clear and rebuild tabs to ensure all customers are present and updated
        indebtednessTab->clear();
        indebtednessTables.clear();
        customerSummaryCards.clear();
        for (const auto& customer : inventory.getAllCustomersWithIndebtedness()) {
            QWidget* customerPage = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(customerPage);
            layout->setContentsMargins(10, 10, 10, 10);

            // Summary Card for Total Owed
            double totalOwed = inventory.getTotalOwedByCustomer(customer);
            QFrame* summaryCard = createSummaryCard("Total Owed",
                QString("LE%1").arg(totalOwed, 0, 'f', 2),
                totalOwed > 0 ? "#e74c3c" : "#2ecc71");
            layout->addWidget(summaryCard);
            customerSummaryCards[customer] = summaryCard;


            // Payment Input Section
            QFrame* paymentFrame = new QFrame();
            paymentFrame->setObjectName("cardFrame");
            QHBoxLayout* paymentLayout = new QHBoxLayout(paymentFrame);
            paymentLayout->setContentsMargins(10, 10, 10, 10);

            // NEW: Date input for payment
            QDateEdit* paymentDateInput = new QDateEdit(QDate::currentDate(), paymentFrame);
            paymentDateInput->setCalendarPopup(true);
            paymentDateInput->setDisplayFormat("yyyy-MM-dd"); // Use YYYY-MM-DD
            paymentDateInput->setObjectName("styledInput");

            QLineEdit* paymentAmountInput = new QLineEdit();
            paymentAmountInput->setObjectName("styledInput");
            paymentAmountInput->setPlaceholderText("Payment amount");
            paymentAmountInput->setValidator(new QDoubleValidator(0.0, 999999.99, 2, paymentAmountInput));

            QPushButton* paymentButton = new QPushButton("Add Payment");
            paymentButton->setObjectName("successButton");
            paymentButton->setMinimumHeight(40);

            paymentLayout->addWidget(paymentDateInput);
            paymentLayout->addWidget(paymentAmountInput);
            paymentLayout->addWidget(paymentButton);

            /* TRANSACTIONS TABLE */
            QTableWidget* table = new QTableWidget();
            table->setObjectName("styledTable");
            table->setColumnCount(6); // Date, Goods, Amount Owed, Paid, Remaining, Payment Details
            table->setHorizontalHeaderLabels({ "Date", "Goods", "Amount Owed", "Paid", "Remaining", "Payment Details" });
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setAlternatingRowColors(true);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->verticalHeader()->setVisible(false);
            layout->addWidget(table);
            indebtednessTables[customer] = table;

            // Connect payment button
            connect(paymentButton, &QPushButton::clicked, this, [=]() {
                // Determine which debt entry to apply payment to
                // For simplicity, apply to the first outstanding debt or allow user to select
                // Here, we'll try to find the currently selected row's debtId
                int currentRow = table->currentRow();
                if (currentRow != -1) {
                    const auto& entries = inventory.getIndebtedness(customer); // Re-fetch for current data
                    if (currentRow < entries.size()) {
                        int debtId = entries[currentRow].debtId;
                        addIndebtednessPaymentToCustomer(customer, paymentDateInput, paymentAmountInput, summaryCard, table); // Pass debtId if function is modified to use it
                    }
                    else {
                        QMessageBox::warning(this, "Selection Error", "Please select a valid debt entry.");
                    }
                }
                else {
                    QMessageBox::warning(this, "Selection Error", "Please select a debt entry to apply payment to.");
                }
                });

            // Connect for showing payment history details
            connect(table, &QTableWidget::cellDoubleClicked, this,
                [=](int r, int c) {
                    if (c == 5) { // If "Payment Details" column is double-clicked
                        const auto& entries = inventory.getIndebtedness(customer);
                        if (r >= 0 && r < entries.size()) {
                            showIndebtednessPaymentHistory(customer, entries[r].debtId); // Pass the debt ID
                        }
                    }
                });

            indebtednessTab->addTab(customerPage, QString::fromStdString(customer));
            refreshIndebtednessForCustomer(customer);
        }
        });

    // Populate tabs initially
    for (const auto& customer : inventory.getAllCustomersWithIndebtedness()) {
        QWidget* customerPage = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(customerPage);
        layout->setContentsMargins(10, 10, 10, 10);

        // Summary Card for Total Owed
        double totalOwed = inventory.getTotalOwedByCustomer(customer);
        QFrame* summaryCard = createSummaryCard("Total Owed",
            QString("LE%1").arg(totalOwed, 0, 'f', 2),
            totalOwed > 0 ? "#e74c3c" : "#2ecc71");
        layout->addWidget(summaryCard);
        customerSummaryCards[customer] = summaryCard;


        // Payment Input Section
        QFrame* paymentFrame = new QFrame();
        paymentFrame->setObjectName("cardFrame");
        QHBoxLayout* paymentLayout = new QHBoxLayout(paymentFrame);
        paymentLayout->setContentsMargins(10, 10, 10, 10);

        // NEW: Date input for payment
        QDateEdit* paymentDateInput = new QDateEdit(QDate::currentDate());
        paymentDateInput->setCalendarPopup(true);
        paymentDateInput->setDisplayFormat("yyyy-MM-dd"); // Use YYYY-MM-DD
        paymentDateInput->setObjectName("styledInput");


        QLineEdit* paymentInput = new QLineEdit();
        paymentInput->setObjectName("styledInput");
        paymentInput->setPlaceholderText("Enter payment amount");
        paymentInput->setValidator(new QDoubleValidator(0.0, 999999.99, 2, paymentInput));

        QPushButton* payButton = new QPushButton("Add Payment");
        payButton->setObjectName("successButton");
        payButton->setMinimumHeight(40);
        payButton->setCursor(Qt::PointingHandCursor);

        paymentLayout->addWidget(paymentDateInput);
        paymentLayout->addWidget(paymentInput);
        paymentLayout->addWidget(payButton);
        layout->addWidget(paymentFrame);


        QTableWidget* table = new QTableWidget();
        table->setObjectName("styledTable");
        table->setColumnCount(6); // Date, Goods, Amount Owed, Paid, Remaining, Payment Details
        table->setHorizontalHeaderLabels({ "Date", "Goods", "Amount Owed", "Paid", "Remaining", "Payment Details" });
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setAlternatingRowColors(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->verticalHeader()->setVisible(false);
        layout->addWidget(table);
        indebtednessTables[customer] = table;

        // Connect payment button
        connect(payButton, &QPushButton::clicked, this, [=]() {
            // This button now needs to figure out which debt entry to update.
            // It's more complex if there are multiple debts for one customer.
            // For now, we'll assume it applies to the selected row, or the oldest outstanding debt.
            // A better UI might have a "Pay" button per row.
            int currentRow = table->currentRow();
            if (currentRow != -1) {
                const auto& entries = inventory.getIndebtedness(customer);
                if (currentRow < entries.size()) {
                    int debtId = entries[currentRow].debtId;
                    addIndebtednessPaymentToCustomer(customer, paymentDateInput, paymentInput, summaryCard, table); // This needs the debtId
                }
                else {
                    QMessageBox::warning(this, "Selection Error", "Please select a valid debt entry.");
                }
            }
            else {
                QMessageBox::warning(this, "Selection Error", "Please select a debt entry to apply payment to.");
            }
            });

        // Connect for showing payment history details
        connect(table, &QTableWidget::cellDoubleClicked, this,
            [=](int r, int c) {
                if (c == 5) { // If "Payment Details" column is double-clicked
                    const auto& entries = inventory.getIndebtedness(customer);
                    if (r >= 0 && r < entries.size()) {
                        showIndebtednessPaymentHistory(customer, entries[r].debtId); // Pass the debt ID
                    }
                }
            });

        indebtednessTab->addTab(customerPage, QString::fromStdString(customer));
        refreshIndebtednessForCustomer(customer);
    }

    tabWidget->addTab(indebtedTab, "💳 Indebtedness");
}

// Fixed: Re-added the implementation for handleAddIndebtedness
void MainWindow::handleAddIndebtedness() {
    QString customer = debtCustomerInput->text().trimmed();
    QString date = debtDateInput->date().toString("yyyy-MM-dd"); // Use YYYY-MM-DD for consistency
    double amount = debtAmountInput->text().toDouble();
    QString goods = debtGoodsInput->text().trimmed();

    if (customer.isEmpty() || goods.isEmpty() || amount <= 0) {
        QMessageBox::warning(this, "Input Error", "Please fill all fields correctly.");
        return;
    }

    // Initialize with 0 paid amount and empty payment history
    IndebtednessEntry entry{ 0, date.toStdString(), customer.toStdString(), goods.toStdString(), amount, 0.0, {} }; // debtId will be auto-incremented by DB
    inventory.addIndebtedness(customer.toStdString(), entry);

    // If this is a new customer, add a new tab
    if (indebtednessTables.find(customer.toStdString()) == indebtednessTables.end()) {
        QWidget* customerPage = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(customerPage);
        layout->setContentsMargins(10, 10, 10, 10);

        // Summary Card for Total Owed
        double totalOwed = inventory.getTotalOwedByCustomer(customer.toStdString());
        QFrame* summaryCard = createSummaryCard("Total Owed",
            QString("LE%1").arg(totalOwed, 0, 'f', 2),
            totalOwed > 0 ? "#e74c3c" : "#2ecc71");
        layout->addWidget(summaryCard);
        customerSummaryCards[customer.toStdString()] = summaryCard;


        // Payment Input Section
        QFrame* paymentFrame = new QFrame();
        paymentFrame->setObjectName("cardFrame");
        QHBoxLayout* paymentLayout = new QHBoxLayout(paymentFrame);
        paymentLayout->setContentsMargins(10, 10, 10, 10);

        // NEW: Date input for payment
        QDateEdit* paymentDateInput = new QDateEdit(QDate::currentDate());
        paymentDateInput->setCalendarPopup(true);
        paymentDateInput->setDisplayFormat("yyyy-MM-dd"); // Use YYYY-MM-DD
        paymentDateInput->setObjectName("styledInput");


        QLineEdit* paymentInput = new QLineEdit();
        paymentInput->setObjectName("styledInput");
        paymentInput->setPlaceholderText("Enter payment amount");
        paymentInput->setValidator(new QDoubleValidator(0.0, 999999.99, 2, paymentInput));

        QPushButton* payButton = new QPushButton("Add Payment");
        payButton->setObjectName("successButton");
        payButton->setMinimumHeight(40);
        payButton->setCursor(Qt::PointingHandCursor);

        paymentLayout->addWidget(paymentDateInput);
        paymentLayout->addWidget(paymentInput);
        paymentLayout->addWidget(payButton);
        layout->addWidget(paymentFrame);


        QTableWidget* table = new QTableWidget();
        table->setObjectName("styledTable");
        table->setColumnCount(6); // Date, Goods, Amount Owed, Paid, Remaining, Payment Details
        table->setHorizontalHeaderLabels({ "Date", "Goods", "Amount Owed", "Paid", "Remaining", "Payment Details" });
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setAlternatingRowColors(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->verticalHeader()->setVisible(false);
        layout->addWidget(table);
        indebtednessTables[customer.toStdString()] = table;

        // Connect payment button
        connect(payButton, &QPushButton::clicked, this, [=]() {
            // Apply payment to the selected debt entry or first outstanding one.
            // For now, let's assume it applies to the selected row's debtId
            int currentRow = table->currentRow();
            if (currentRow != -1) {
                const auto& entries = inventory.getIndebtedness(customer.toStdString());
                if (currentRow < entries.size()) {
                    int debtId = entries[currentRow].debtId;
                    addIndebtednessPaymentToCustomer(customer.toStdString(), paymentDateInput, paymentInput, summaryCard, table);
                }
                else {
                    QMessageBox::warning(this, "Selection Error", "Please select a valid debt entry.");
                }
            }
            else {
                QMessageBox::warning(this, "Selection Error", "Please select a debt entry to apply payment to.");
            }
            });

        // Connect for showing payment history details
        connect(table, &QTableWidget::cellDoubleClicked, this,
            [=](int r, int c) {
                if (c == 5) { // If "Payment Details" column is double-clicked
                    const auto& entries = inventory.getIndebtedness(customer.toStdString());
                    if (r >= 0 && r < entries.size()) {
                        showIndebtednessPaymentHistory(customer.toStdString(), entries[r].debtId);
                    }
                }
            });

        indebtednessTab->addTab(customerPage, customer);
    }

    refreshIndebtednessForCustomer(customer.toStdString());

    debtCustomerInput->clear();
    debtAmountInput->clear();
    debtGoodsInput->clear();
    debtDateInput->setDate(QDate::currentDate());
}

// NEW: Add indebtedness payment to customer
void MainWindow::addIndebtednessPaymentToCustomer(const std::string& customerName, QDateEdit* paymentDateInput, QLineEdit* paymentInput, QFrame* summaryCard, QTableWidget* table) {
    bool ok;
    double amount = paymentInput->text().toDouble(&ok);
    QString date = paymentDateInput->date().toString("yyyy-MM-dd");

    if (ok && amount > 0) {
        // Find the debtId of the currently selected row in the table
        int selectedRow = table->currentRow();
        if (selectedRow == -1) {
            QMessageBox::warning(this, "Selection Required", "Please select a specific debt entry to apply the payment to.");
            return;
        }

        const auto& entries = inventory.getIndebtedness(customerName);
        if (selectedRow >= entries.size()) {
            QMessageBox::warning(this, "Error", "Invalid debt entry selected.");
            return;
        }
        int debtId = entries[selectedRow].debtId;

        if (inventory.addIndebtednessPayment(customerName, date.toStdString(), amount, debtId)) {
            refreshIndebtednessForCustomer(customerName);
            double newTotalOwed = inventory.getTotalOwedByCustomer(customerName);
            // Update summary card value and color
            QLabel* valueLabel = summaryCard->findChild<QLabel*>("valueLabel");
            if (valueLabel) {
                valueLabel->setText(QString("LE%1").arg(newTotalOwed, 0, 'f', 2));
                summaryCard->setStyleSheet(QString(
                    "background-color: %1;"
                    "border-radius: 8px;"
                    "padding: 15px;"
                    "color: white;"
                ).arg(newTotalOwed > 0 ? "#e74c3c" : "#2ecc71"));
            }
            paymentInput->clear();
            QMessageBox::information(this, "Payment Recorded", QString("Payment of LE%1 recorded for %2 on %3.").arg(amount, 0, 'f', 2).arg(QString::fromStdString(customerName)).arg(date));
        }
        else {
            QMessageBox::warning(this, "Payment Error", "Failed to record payment. Customer not found or no outstanding debt for selected entry.");
        }
    }
    else {
        QMessageBox::warning(this, "Input Error", "Please enter a valid payment amount.");
    }
}

// NEW: Show indebtedness payment history for a specific transaction
void MainWindow::showIndebtednessPaymentHistory(const std::string& customerName, int debtId) {
    // Fetch the specific IndebtednessEntry from the database using debtId
    IndebtednessEntry currentEntry;
    bool found = false;
    for (const auto& entry : inventory.getIndebtedness(customerName)) {
        if (entry.debtId == debtId) {
            currentEntry = entry;
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(this, "Error", "Debt entry not found.");
        return;
    }

    QDialog detailsDialog(this);
    detailsDialog.setWindowTitle(QString("Payment History - %1 (%2)").arg(QString::fromStdString(customerName)).arg(QString::fromStdString(currentEntry.goodsDescription)));
    detailsDialog.resize(500, 400);

    QVBoxLayout* layout = new QVBoxLayout(&detailsDialog);

    // Transaction summary
    QLabel* summaryLabel = new QLabel(
        QString("<b>Debt Record on %1</b><br>"
            "Goods: <b>%2</b><br>"
            "Original Amount Owed: <b>LE%3</b><br>"
            "Total Paid: <b>LE%4</b><br>"
            "Remaining Owed: <b>LE%5</b>")
        .arg(QString::fromStdString(currentEntry.date))
        .arg(QString::fromStdString(currentEntry.goodsDescription))
        .arg(currentEntry.amountOwed, 0, 'f', 2)
        .arg(currentEntry.amountPaid, 0, 'f', 2)
        .arg(currentEntry.getRemainingOwed(), 0, 'f', 2)
    );
    layout->addWidget(summaryLabel);

    // Payment history table
    QTableWidget* paymentTable = new QTableWidget();
    paymentTable->setColumnCount(2);
    paymentTable->setHorizontalHeaderLabels({ "Date", "Amount" });
    paymentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    const auto& paymentHistory = inventory.getIndebtednessPaymentHistoryForEntry(currentEntry.debtId);
    paymentTable->setRowCount(paymentHistory.size());

    for (int i = 0; i < paymentHistory.size(); ++i) {
        const auto& payment = paymentHistory[i];
        paymentTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(payment.date)));
        paymentTable->setItem(i, 1, new QTableWidgetItem(QString("LE%1").arg(payment.amount, 0, 'f', 2)));
        paymentTable->item(i, 1)->setForeground(Qt::darkGreen);
    }

    layout->addWidget(paymentTable);
    detailsDialog.exec();
}

void MainWindow::showPaymentHistory(const std::string& supplierName, int transactionId) {
    // Fetch the specific SupplierTransaction from the database using transactionId
    SupplierTransaction currentTx;
    bool found = false;
    for (const auto& tx : inventory.getSupplierTransactionsForSupplier(supplierName)) {
        if (tx.transactionId == transactionId) {
            currentTx = tx;
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(this, "Error", "Supplier transaction not found.");
        return;
    }

    QDialog detailsDialog(this);
    detailsDialog.setWindowTitle(QString("Payment History - %1").arg(QString::fromStdString(currentTx.supplierName)));
    detailsDialog.resize(500, 400);

    QVBoxLayout* layout = new QVBoxLayout(&detailsDialog);

    // Transaction summary
    QLabel* summaryLabel = new QLabel(
        QString("<b>Transaction on %1</b><br>"
            "Total: <b>LE%2</b><br>"
            "Paid: <b>LE%3</b><br>"
            "Remaining: <b>LE%4</b>")
        .arg(QString::fromStdString(currentTx.date))
        .arg(currentTx.totalAmount, 0, 'f', 2)
        .arg(currentTx.paidAmount, 0, 'f', 2)
        .arg(currentTx.getRemainingAmount(), 0, 'f', 2)
    );
    layout->addWidget(summaryLabel);

    // Payment history table
    QTableWidget* paymentTable = new QTableWidget();
    paymentTable->setColumnCount(2);
    paymentTable->setHorizontalHeaderLabels({ "Date", "Amount" });
    paymentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    const auto& paymentHistory = inventory.getSupplierPaymentHistoryForTransaction(currentTx.transactionId);
    paymentTable->setRowCount(paymentHistory.size());

    for (int i = 0; i < paymentHistory.size(); ++i) {
        const auto& payment = paymentHistory[i];
        paymentTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(payment.date)));
        paymentTable->setItem(i, 1, new QTableWidgetItem(QString("LE%1").arg(payment.amount, 0, 'f', 2)));
        paymentTable->item(i, 1)->setForeground(Qt::darkGreen);
    }

    layout->addWidget(paymentTable);
    detailsDialog.exec();
}


void MainWindow::showInventoryManagement() {
    bool ok;
    QString password = QInputDialog::getText(this, "Admin Access",
        "Enter admin password:", QLineEdit::Password,
        "", &ok);

    if (ok && !password.isEmpty() && password == "25254") {
        QDialog* dialog = new QDialog(this);
        dialog->setWindowTitle("Inventory Management");
        dialog->setMinimumSize(800, 600);
        dialog->setStyleSheet("QDialog { background-color: #f5f5f5; }");

        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(20);

        QHBoxLayout* contentLayout = new QHBoxLayout();
        contentLayout->setSpacing(20);

        // Left side - Add Item Form
        QFrame* formFrame = new QFrame();
        formFrame->setObjectName("cardFrame");
        formFrame->setFixedWidth(320);
        formFrame->setMaximumHeight(500);

        QVBoxLayout* formLayout = new QVBoxLayout(formFrame);
        formLayout->setContentsMargins(25, 25, 25, 25);
        formLayout->setSpacing(20);

        QLabel* formTitle = new QLabel("Add New Item");
        formTitle->setObjectName("sectionTitle");

        QVBoxLayout* fieldsLayout = new QVBoxLayout();
        fieldsLayout->setSpacing(15);

        QVBoxLayout* idLayout = new QVBoxLayout();
        idLayout->setSpacing(5);
        QLabel* idLabel = new QLabel("Item ID");
        idLabel->setObjectName("fieldLabel");
        itemIdInput = new QLineEdit();
        itemIdInput->setObjectName("styledInput");
        itemIdInput->setPlaceholderText("e.g., BOOK001");
        idLayout->addWidget(idLabel);
        idLayout->addWidget(itemIdInput);

        // NEW: Auto-tabbing for itemIdInput in Inventory Management
        connect(itemIdInput, &QLineEdit::returnPressed, this, [=]() {
            if (!itemIdInput->text().isEmpty()) {
                itemNameInput->setFocus(); // Move to name only when Enter is pressed (or scan completes)
            }
            });

        QVBoxLayout* nameLayout = new QVBoxLayout();
        nameLayout->setSpacing(5);
        QLabel* nameLabel = new QLabel("Item Name");
        nameLabel->setObjectName("fieldLabel");
        itemNameInput = new QLineEdit();
        itemNameInput->setObjectName("styledInput");
        itemNameInput->setPlaceholderText("e.g., Blue Notebooks");
        nameLayout->addWidget(nameLabel);
        nameLayout->addWidget(itemNameInput);

        QVBoxLayout* quantityLayout = new QVBoxLayout();
        quantityLayout->setSpacing(5);
        QLabel* quantityLabel = new QLabel("Quantity");
        quantityLabel->setObjectName("fieldLabel");
        itemQuantityInput = new QLineEdit();
        itemQuantityInput->setObjectName("styledInput");
        itemQuantityInput->setPlaceholderText("e.g., 25");
        quantityLayout->addWidget(quantityLabel);
        quantityLayout->addWidget(itemQuantityInput);

        QVBoxLayout* priceLayout = new QVBoxLayout();
        priceLayout->setSpacing(5);
        QLabel* priceLabel = new QLabel("Sell Price (LE)");
        priceLabel->setObjectName("fieldLabel");
        itemPriceInput = new QLineEdit();
        itemPriceInput->setObjectName("styledInput");
        itemPriceInput->setPlaceholderText("e.g., 15.99");
        priceLayout->addWidget(priceLabel);
        priceLayout->addWidget(itemPriceInput);

        // New: Bought Price Input
        QVBoxLayout* boughtPriceLayout = new QVBoxLayout();
        boughtPriceLayout->setSpacing(5);
        QLabel* boughtPriceLabel = new QLabel("Bought Price (LE)");
        boughtPriceLabel->setObjectName("fieldLabel");
        boughtPriceInput = new QLineEdit();
        boughtPriceInput->setObjectName("styledInput");
        boughtPriceInput->setPlaceholderText("e.g., 10.00");
        boughtPriceLayout->addWidget(boughtPriceLabel);
        boughtPriceLayout->addWidget(boughtPriceInput);

        // New: Low-stock Threshold Input
        QVBoxLayout* thresholdLayout = new QVBoxLayout();
        thresholdLayout->setSpacing(5);
        QLabel* thresholdLabel = new QLabel("Low-Stock Threshold");
        thresholdLabel->setObjectName("fieldLabel");
        itemThresholdInput = new QLineEdit();
        itemThresholdInput->setObjectName("styledInput");
        itemThresholdInput->setPlaceholderText("e.g., 5  (alert when qty < this)");
        itemThresholdInput->setValidator(new QIntValidator(0, 99999, dialog));
        thresholdLayout->addWidget(thresholdLabel);
        thresholdLayout->addWidget(itemThresholdInput);

        fieldsLayout->addLayout(idLayout);
        fieldsLayout->addLayout(nameLayout);
        fieldsLayout->addLayout(quantityLayout);
        fieldsLayout->addLayout(priceLayout);
        fieldsLayout->addLayout(boughtPriceLayout);
        fieldsLayout->addLayout(thresholdLayout);

        addItemButton = new QPushButton("Add Item to Inventory");
        addItemButton->setObjectName("primaryButton");
        addItemButton->setMinimumHeight(45);
        addItemButton->setDefault(false);
        addItemButton->setAutoDefault(false);
        addItemButton->setCursor(Qt::PointingHandCursor);
        //addItemButton->setDefault(true); // Set as default for this dialog

        formLayout->addWidget(formTitle);
        formLayout->addLayout(fieldsLayout);
        formLayout->addWidget(addItemButton);
        formLayout->addStretch();

        // Right side - Inventory Table
        QFrame* tableFrame = new QFrame();
        tableFrame->setObjectName("cardFrame");

        QVBoxLayout* tableLayout = new QVBoxLayout(tableFrame);
        tableLayout->setContentsMargins(25, 25, 25, 25);
        tableLayout->setSpacing(15);

        QLabel* tableTitle = new QLabel("Current Inventory");
        tableTitle->setObjectName("sectionTitle");

        QTableWidget* inventoryTableDialog = new QTableWidget();
        inventoryTableDialog->setObjectName("styledTable");
        inventoryTableDialog->setColumnCount(7);
        inventoryTableDialog->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Quantity", "Sell Price", "Bought Price", "Total Sell Value", "Threshold" });
        inventoryTableDialog->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        inventoryTableDialog->setAlternatingRowColors(true);
        inventoryTableDialog->setSelectionBehavior(QAbstractItemView::SelectRows);
        inventoryTableDialog->verticalHeader()->setVisible(false);

        // Populate the table (optionally filtered by search term)
        auto refreshInventoryTable = [=](const QString& filterText = "") {
            const auto& inv = inventory.getInventory();
            QString lower = filterText.trimmed().toLower();

            inventoryTableDialog->setRowCount(0);
            int row = 0;
            for (const auto& pair : inv) {
                const Item& item = pair.second;
                QString itemId = QString::fromStdString(item.id);
                QString itemName = QString::fromStdString(item.name);

                // Filter: show all when empty, otherwise match ID or name
                if (!lower.isEmpty() &&
                    !itemId.toLower().contains(lower) &&
                    !itemName.toLower().contains(lower)) {
                    continue;
                }

                double totalSellValue = item.quantity * item.price;
                inventoryTableDialog->insertRow(row);
                inventoryTableDialog->setItem(row, 0, new QTableWidgetItem(itemId));
                inventoryTableDialog->setItem(row, 1, new QTableWidgetItem(itemName));
                inventoryTableDialog->setItem(row, 2, new QTableWidgetItem(QString::number(item.quantity)));
                inventoryTableDialog->setItem(row, 3, new QTableWidgetItem(QString("LE%1").arg(item.price, 0, 'f', 2)));
                inventoryTableDialog->setItem(row, 4, new QTableWidgetItem(QString("LE%1").arg(item.boughtPrice, 0, 'f', 2)));
                inventoryTableDialog->setItem(row, 5, new QTableWidgetItem(QString("LE%1").arg(totalSellValue, 0, 'f', 2)));
                inventoryTableDialog->setItem(row, 6, new QTableWidgetItem(QString::number(item.threshold)));

                // Highlight low-stock rows
                if (item.quantity < item.threshold) {
                    for (int col = 0; col < 7; ++col) {
                        if (inventoryTableDialog->item(row, col)) {
                            inventoryTableDialog->item(row, col)->setBackground(QColor(255, 240, 240));
                            inventoryTableDialog->item(row, col)->setForeground(Qt::darkRed);
                        }
                    }
                }
                row++;
            }
            };

        // ── Search bar above the table ────────────────────────────────────────
        QHBoxLayout* searchBarLayout = new QHBoxLayout();
        searchBarLayout->setSpacing(8);

        QLineEdit* invSearchInput = new QLineEdit();
        invSearchInput->setObjectName("styledInput");
        invSearchInput->setPlaceholderText("🔍  Search by item ID or name...");
        invSearchInput->setMinimumHeight(38);
        invSearchInput->setClearButtonEnabled(true);

        QPushButton* invSearchClearBtn = new QPushButton("✕  Clear");
        invSearchClearBtn->setObjectName("secondaryButton");
        invSearchClearBtn->setMinimumHeight(38);
        invSearchClearBtn->setMinimumWidth(80);
        invSearchClearBtn->setAutoDefault(false);
        invSearchClearBtn->setCursor(Qt::PointingHandCursor);

        searchBarLayout->addWidget(invSearchInput, 1);
        searchBarLayout->addWidget(invSearchClearBtn);

        // Live filtering as user types
        connect(invSearchInput, &QLineEdit::textChanged, this,
            [=](const QString& text) {
                refreshInventoryTable(text);
            });

        // Clear button resets search and reloads full list
        connect(invSearchClearBtn, &QPushButton::clicked, this,
            [=]() {
                invSearchInput->clear();
                refreshInventoryTable();
                invSearchInput->setFocus();
            });

        // Initial table population (full list)
        refreshInventoryTable();

        tableLayout->addWidget(tableTitle);
        tableLayout->addLayout(searchBarLayout);
        tableLayout->addWidget(inventoryTableDialog);

        // New: Buttons for total sell and bought money
        QHBoxLayout* totalsLayout = new QHBoxLayout();
        totalsLayout->setSpacing(10);
        totalsLayout->addStretch();

        QPushButton* showTotalSellMoneyButton = new QPushButton("Show Total Sell Money");
        showTotalSellMoneyButton->setObjectName("secondaryButton");
        showTotalSellMoneyButton->setMinimumHeight(40);
        showTotalSellMoneyButton->setCursor(Qt::PointingHandCursor);
        showTotalSellMoneyButton->setAutoDefault(false); // Prevent Enter key activation

        QPushButton* showTotalBoughtMoneyButton = new QPushButton("Show Total Bought Money");
        showTotalBoughtMoneyButton->setObjectName("secondaryButton");
        showTotalBoughtMoneyButton->setMinimumHeight(40);
        showTotalBoughtMoneyButton->setCursor(Qt::PointingHandCursor);
        showTotalBoughtMoneyButton->setAutoDefault(false); // Prevent Enter key activation

        totalsLayout->addWidget(showTotalSellMoneyButton);
        totalsLayout->addWidget(showTotalBoughtMoneyButton);
        tableLayout->addLayout(totalsLayout);

        // ── Edit / Delete buttons ─────────────────────────────────────────────
        QHBoxLayout* editDeleteLayout = new QHBoxLayout();
        editDeleteLayout->setSpacing(10);

        QPushButton* editItemButton = new QPushButton("✏️  Edit Selected Item");
        editItemButton->setObjectName("primaryButton");
        editItemButton->setMinimumHeight(40);
        editItemButton->setCursor(Qt::PointingHandCursor);
        editItemButton->setAutoDefault(false);

        QPushButton* deleteItemButton = new QPushButton("🗑️  Delete Selected Item");
        deleteItemButton->setObjectName("dangerButton");
        deleteItemButton->setMinimumHeight(40);
        deleteItemButton->setCursor(Qt::PointingHandCursor);
        deleteItemButton->setAutoDefault(false);

        editDeleteLayout->addStretch();
        editDeleteLayout->addWidget(editItemButton);
        editDeleteLayout->addWidget(deleteItemButton);
        tableLayout->addLayout(editDeleteLayout);

        // Right-click context menu on the table
        inventoryTableDialog->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(inventoryTableDialog, &QTableWidget::customContextMenuRequested, this,
            [=](const QPoint& pos) {
                QTableWidgetItem* clickedCell = inventoryTableDialog->itemAt(pos);
                if (!clickedCell) return;
                int row = clickedCell->row();
                QString itemId = inventoryTableDialog->item(row, 0)->text();

                QMenu contextMenu(inventoryTableDialog);
                QAction* editAction = contextMenu.addAction("✏️  Edit Item");
                QAction* deleteAction = contextMenu.addAction("🗑️  Delete Item");
                QAction* chosen = contextMenu.exec(inventoryTableDialog->viewport()->mapToGlobal(pos));
                if (chosen == editAction)
                    editInventoryItem(itemId, inventoryTableDialog,
                        [=]() { refreshInventoryTable(invSearchInput->text()); });
                else if (chosen == deleteAction)
                    deleteInventoryItem(itemId, inventoryTableDialog,
                        [=]() { refreshInventoryTable(invSearchInput->text()); });
            });

        // Connect Edit button
        connect(editItemButton, &QPushButton::clicked, this, [=]() {
            int row = inventoryTableDialog->currentRow();
            if (row < 0) {
                QMessageBox::warning(dialog, "No Selection", "Please select an item row to edit.");
                return;
            }
            QString itemId = inventoryTableDialog->item(row, 0)->text();
            editInventoryItem(itemId, inventoryTableDialog,
                [=]() { refreshInventoryTable(invSearchInput->text()); });
            });

        // Connect Delete button
        connect(deleteItemButton, &QPushButton::clicked, this, [=]() {
            int row = inventoryTableDialog->currentRow();
            if (row < 0) {
                QMessageBox::warning(dialog, "No Selection", "Please select an item row to delete.");
                return;
            }
            QString itemId = inventoryTableDialog->item(row, 0)->text();
            deleteInventoryItem(itemId, inventoryTableDialog,
                [=]() { refreshInventoryTable(invSearchInput->text()); });
            });
        // ─────────────────────────────────────────────────────────────────────

        // Connect the Add Item button
        connect(addItemButton, &QPushButton::clicked, this, [=, &dialog]() {
            QString id = itemIdInput->text().trimmed();
            QString name = itemNameInput->text().trimmed();
            QString quantityStr = itemQuantityInput->text().trimmed();
            QString priceStr = itemPriceInput->text().trimmed();
            QString boughtPriceStr = boughtPriceInput->text().trimmed();

            if (id.isEmpty() || name.isEmpty() || quantityStr.isEmpty() || priceStr.isEmpty() || boughtPriceStr.isEmpty()) {
                QMessageBox::warning(dialog, "Error", "All fields are required");
                return;
            }

            bool okQuantity, okSellPrice, okBoughtPrice;
            int quantity = quantityStr.toInt(&okQuantity);
            if (!okQuantity || quantity <= 0) {
                QMessageBox::warning(dialog, "Error", "Quantity must be a positive integer");
                return;
            }

            double price = priceStr.toDouble(&okSellPrice);
            if (!okSellPrice || price <= 0) {
                QMessageBox::warning(dialog, "Error", "Sell Price must be a positive number");
                return;
            }

            double boughtPrice = boughtPriceStr.toDouble(&okBoughtPrice);
            if (!okBoughtPrice || boughtPrice < 0) {
                QMessageBox::warning(dialog, "Error", "Bought Price must be a non-negative number");
                return;
            }

            // -- Duplicate barcode check ------------------------------------------
            QSqlQuery checkQuery;
            checkQuery.prepare("SELECT item_name, quantity, price, bought_price "
                "FROM items WHERE item_id = :item_id");
            checkQuery.bindValue(":item_id", id);

            if (checkQuery.exec() && checkQuery.next()) {
                QString existingName = checkQuery.value("item_name").toString();
                int     existingQty = checkQuery.value("quantity").toInt();
                double  existingPrice = checkQuery.value("price").toDouble();
                double  existingBought = checkQuery.value("bought_price").toDouble();

                // ── Styled duplicate-barcode dialog ──────────────────────────
                QDialog dupDialog(dialog);
                dupDialog.setWindowTitle("Barcode Already Exists");
                dupDialog.setFixedWidth(460);
                dupDialog.setStyleSheet(R"(
                    QDialog {
                        background-color: #f8f9fa;
                        font-family: 'Segoe UI', Arial, sans-serif;
                    }
                    QLabel#dupTitle {
                        font-size: 17px;
                        font-weight: bold;
                        color: white;
                    }
                    QLabel#dupBody {
                        font-size: 13px;
                        color: #2c3e50;
                        background: white;
                        border-radius: 10px;
                        padding: 16px;
                        border: 1px solid #e9ecef;
                    }
                    QPushButton#editBtn {
                        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                            stop:0 #007bff, stop:1 #0056b3);
                        color: white; border: none; border-radius: 8px;
                        font-size: 13px; font-weight: 600;
                        padding: 10px 20px; min-height: 38px;
                    }
                    QPushButton#editBtn:hover {
                        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                            stop:0 #0056b3, stop:1 #004085);
                    }
                    QPushButton#deleteBtn {
                        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                            stop:0 #dc3545, stop:1 #c82333);
                        color: white; border: none; border-radius: 8px;
                        font-size: 13px; font-weight: 600;
                        padding: 10px 20px; min-height: 38px;
                    }
                    QPushButton#deleteBtn:hover {
                        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                            stop:0 #c82333, stop:1 #bd2130);
                    }
                    QPushButton#cancelBtn {
                        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                            stop:0 #6c757d, stop:1 #545b62);
                        color: white; border: none; border-radius: 8px;
                        font-size: 13px; font-weight: 600;
                        padding: 10px 20px; min-height: 38px;
                    }
                    QPushButton#cancelBtn:hover {
                        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                            stop:0 #545b62, stop:1 #3d4142);
                    }
                )");

                QVBoxLayout* dupLayout = new QVBoxLayout(&dupDialog);
                dupLayout->setContentsMargins(0, 0, 0, 20);
                dupLayout->setSpacing(0);

                // Gradient header banner
                QFrame* headerBanner = new QFrame();
                headerBanner->setFixedHeight(72);
                headerBanner->setStyleSheet(
                    "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                    "stop:0 #667eea, stop:1 #764ba2);"
                    "border-top-left-radius: 4px; border-top-right-radius: 4px;"
                );
                QHBoxLayout* bannerLayout = new QHBoxLayout(headerBanner);
                bannerLayout->setContentsMargins(20, 0, 20, 0);

                QLabel* warnIcon = new QLabel("⚠");
                warnIcon->setStyleSheet("font-size: 28px; color: #ffe066; background: transparent;");

                QLabel* dupTitle = new QLabel("Barcode Already Exists");
                dupTitle->setObjectName("dupTitle");
                dupTitle->setStyleSheet("font-size: 17px; font-weight: bold; color: white; background: transparent;");

                bannerLayout->addWidget(warnIcon);
                bannerLayout->addSpacing(10);
                bannerLayout->addWidget(dupTitle);
                bannerLayout->addStretch();
                dupLayout->addWidget(headerBanner);

                // Info card
                dupLayout->addSpacing(16);
                QLabel* dupBody = new QLabel(
                    QString(
                        "<b style='color:#667eea;'>Barcode: %1</b><br><br>"
                        "<table cellspacing='6'>"
                        "<tr><td style='color:#6c757d;'>📦 &nbsp;Name</td>"
                        "    <td><b>%2</b></td></tr>"
                        "<tr><td style='color:#6c757d;'>🔢 &nbsp;Quantity</td>"
                        "    <td><b>%3</b></td></tr>"
                        "<tr><td style='color:#6c757d;'>💰 &nbsp;Sell Price</td>"
                        "    <td><b>LE %4</b></td></tr>"
                        "<tr><td style='color:#6c757d;'>🛒 &nbsp;Bought Price</td>"
                        "    <td><b>LE %5</b></td></tr>"
                        "</table><br>"
                        "<span style='color:#6c757d; font-size:12px;'>"
                        "Please choose an action below, or cancel to re-enter a different barcode.</span>"
                    )
                    .arg(id).arg(existingName).arg(existingQty)
                    .arg(existingPrice, 0, 'f', 2)
                    .arg(existingBought, 0, 'f', 2)
                );
                dupBody->setObjectName("dupBody");
                dupBody->setTextFormat(Qt::RichText);
                dupBody->setWordWrap(true);
                dupBody->setContentsMargins(20, 0, 20, 0);
                dupBody->setStyleSheet(
                    "font-size: 13px; color: #2c3e50;"
                    "background: white; border-radius: 10px;"
                    "padding: 16px; border: 1px solid #e9ecef;"
                    "margin-left: 16px; margin-right: 16px;"
                );
                dupLayout->addWidget(dupBody);

                // Buttons row
                dupLayout->addSpacing(16);
                QHBoxLayout* btnRow = new QHBoxLayout();
                btnRow->setContentsMargins(16, 0, 16, 0);
                btnRow->setSpacing(10);

                QPushButton* editBtn = new QPushButton("✏️  Edit Existing");
                editBtn->setObjectName("editBtn");
                QPushButton* deleteBtn = new QPushButton("🗑️  Delete Existing");
                deleteBtn->setObjectName("deleteBtn");
                QPushButton* cancelDupBtn = new QPushButton("Cancel");
                cancelDupBtn->setObjectName("cancelBtn");

                btnRow->addWidget(editBtn);
                btnRow->addWidget(deleteBtn);
                btnRow->addStretch();
                btnRow->addWidget(cancelDupBtn);
                dupLayout->addLayout(btnRow);

                // Wire buttons
                int dupResult = 0; // 0=cancel, 1=edit, 2=delete
                connect(editBtn, &QPushButton::clicked, [&]() { dupResult = 1; dupDialog.accept(); });
                connect(deleteBtn, &QPushButton::clicked, [&]() { dupResult = 2; dupDialog.accept(); });
                connect(cancelDupBtn, &QPushButton::clicked, [&]() { dupResult = 0; dupDialog.reject(); });

                dupDialog.exec();

                if (dupResult == 1) {
                    editInventoryItem(id, inventoryTableDialog, refreshInventoryTable);
                }
                else if (dupResult == 2) {
                    deleteInventoryItem(id, inventoryTableDialog, refreshInventoryTable);
                }
                // 0 = cancel: keep form filled so user can correct the barcode
                return;
            }
            // -- No duplicate, safe to insert -------------------------------------

            bool okThreshold = true;
            int threshold = 5;
            QString threshStr = itemThresholdInput->text().trimmed();
            if (!threshStr.isEmpty()) {
                threshold = threshStr.toInt(&okThreshold);
                if (!okThreshold || threshold < 0) {
                    QMessageBox::warning(dialog, "Error", "Threshold must be a non-negative integer.");
                    return;
                }
            }

            inventory.addItem(id.toStdString(), name.toStdString(), quantity, price, boughtPrice, threshold);
            clearInventoryForm();
            invSearchInput->clear();          // clear search so new item is visible
            refreshInventoryTable();
            refreshDeficienciesTable();
            QMessageBox::information(dialog, "Success", "Item added successfully!");
            });

        // Connect "Show Total Sell Money" button with password protection
        connect(showTotalSellMoneyButton, &QPushButton::clicked, this, [=, &dialog]() {
            bool authOk;
            QString adminPassword = QInputDialog::getText(dialog, "Admin Access",
                "Enter admin password to view total sell money:", QLineEdit::Password,
                "", &authOk);

            if (authOk && !adminPassword.isEmpty() && adminPassword == "25254") {
                double totalSell = inventory.getTotalInventorySellValue();
                QMessageBox::information(dialog, "Total Sell Money",
                    QString("Total potential sell value of all items in inventory: LE%1").arg(totalSell, 0, 'f', 2));
            }
            else if (authOk) {
                QMessageBox::warning(dialog, "Access Denied", "Incorrect password!");
            }
            });

        // Connect "Show Total Bought Money" button with password protection
        connect(showTotalBoughtMoneyButton, &QPushButton::clicked, this, [=, &dialog]() {
            bool authOk;
            QString adminPassword = QInputDialog::getText(dialog, "Admin Access",
                "Enter admin password to view total bought money:", QLineEdit::Password,
                "", &authOk);

            if (authOk && !adminPassword.isEmpty() && adminPassword == "25254") {
                double totalBought = inventory.getTotalInventoryBoughtValue();
                QMessageBox::information(dialog, "Total Bought Money",
                    QString("Total cost of all items in inventory: LE%1").arg(totalBought, 0, 'f', 2));
            }
            else if (authOk) {
                QMessageBox::warning(this, "Access Denied", "Incorrect password!");
            }
            });


        // Close button
        QPushButton* closeButton = new QPushButton("Close");
        closeButton->setObjectName("dangerButton");
        closeButton->setMinimumHeight(40);
        closeButton->setMinimumWidth(100);
        closeButton->setCursor(Qt::PointingHandCursor);
        closeButton->setAutoDefault(false); // Prevent Enter key activation

        // Assemble the layout
        contentLayout->addWidget(formFrame);
        contentLayout->addWidget(tableFrame);
        mainLayout->addLayout(contentLayout);
        mainLayout->addWidget(closeButton, 0, Qt::AlignRight);

        // Apply shadow effects
        QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
        shadowEffect->setBlurRadius(15);
        shadowEffect->setXOffset(0);
        shadowEffect->setYOffset(3);
        shadowEffect->setColor(QColor(0, 0, 0, 30));
        formFrame->setGraphicsEffect(shadowEffect);

        QGraphicsDropShadowEffect* tableShadowEffect = new QGraphicsDropShadowEffect(this);
        tableShadowEffect->setBlurRadius(15);
        tableShadowEffect->setXOffset(0);
        tableShadowEffect->setYOffset(3);
        tableShadowEffect->setColor(QColor(0, 0, 0, 30));
        tableFrame->setGraphicsEffect(tableShadowEffect);

        connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);
        // FIX: Connect Enter key to move focus to the next field
        connect(itemIdInput, &QLineEdit::returnPressed, [=]() {
            itemNameInput->setFocus();
            itemNameInput->selectAll(); // Optional: selects text so you can overwrite if needed
            });

        connect(itemNameInput, &QLineEdit::returnPressed, [=]() {
            itemQuantityInput->setFocus();
            itemQuantityInput->selectAll();
            });

        connect(itemQuantityInput, &QLineEdit::returnPressed, [=]() {
            itemPriceInput->setFocus();
            itemPriceInput->selectAll();
            });

        connect(itemPriceInput, &QLineEdit::returnPressed, [=]() {
            boughtPriceInput->setFocus();
            boughtPriceInput->selectAll();
            });

        connect(boughtPriceInput, &QLineEdit::returnPressed, [=]() {
            itemThresholdInput->setFocus();
            itemThresholdInput->selectAll();
            });
        // Hitting Enter on the LAST field triggers the Add button
        connect(itemThresholdInput, &QLineEdit::returnPressed, addItemButton, &QPushButton::click);

        dialog->exec();
        delete dialog;
    }
    else if (ok) {
        QMessageBox::warning(this, "Access Denied", "Incorrect password!");
    }
}

// NEW: Setup Returns Tab
void MainWindow::setupReturnsTab() {
    QWidget* returnsTab = new QWidget();
    QHBoxLayout* mainLayout = new QHBoxLayout(returnsTab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // Left side - Process Return Form
    QFrame* formFrame = new QFrame();
    formFrame->setObjectName("cardFrame");
    formFrame->setFixedWidth(320);
    formFrame->setMaximumHeight(400);

    QVBoxLayout* formLayout = new QVBoxLayout(formFrame);
    formLayout->setContentsMargins(25, 25, 25, 25);
    formLayout->setSpacing(20);

    QLabel* formTitle = new QLabel("Process Return");
    formTitle->setObjectName("sectionTitle");

    QVBoxLayout* fieldsLayout = new QVBoxLayout();
    fieldsLayout->setSpacing(15);

    QVBoxLayout* itemIdLayout = new QVBoxLayout();
    itemIdLayout->setSpacing(5);
    QLabel* itemIdLabel = new QLabel("Item ID");
    itemIdLabel->setObjectName("fieldLabel");
    returnItemIdInput = new QLineEdit();
    returnItemIdInput->setObjectName("styledInput");
    returnItemIdInput->setPlaceholderText("Enter item ID");
    itemIdLayout->addWidget(itemIdLabel);
    itemIdLayout->addWidget(returnItemIdInput);

    // Move focus to quantity only when Enter is pressed
    connect(returnItemIdInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = returnItemIdInput->text().trimmed();
        if (text.isEmpty()) return;
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM items WHERE item_id = :item_id");
        query.bindValue(":item_id", text);
        if (query.exec() && query.next() && query.value(0).toInt() > 0) {
            returnQuantityInput->setFocus();
        }
        else {
            QMessageBox::warning(this, "Item Not Found",
                QString("Item ID '%1' was not found in inventory.").arg(text));
            returnItemIdInput->selectAll();
        }
        });

    QVBoxLayout* quantityLayout = new QVBoxLayout();
    quantityLayout->setSpacing(5);
    QLabel* quantityLabel = new QLabel("Quantity");
    quantityLabel->setObjectName("fieldLabel");
    returnQuantityInput = new QLineEdit();
    returnQuantityInput->setObjectName("styledInput");
    returnQuantityInput->setPlaceholderText("Enter quantity");
    quantityLayout->addWidget(quantityLabel);
    quantityLayout->addWidget(returnQuantityInput);

    // Enter on quantity moves to reason field
    connect(returnQuantityInput, &QLineEdit::returnPressed, this, [this]() {
        returnReasonInput->setFocus();
        });

    QVBoxLayout* reasonLayout = new QVBoxLayout();
    reasonLayout->setSpacing(5);
    QLabel* reasonLabel = new QLabel("Reason for Return (optional)");
    reasonLabel->setObjectName("fieldLabel");
    returnReasonInput = new QTextEdit();
    returnReasonInput->setObjectName("styledInput");
    returnReasonInput->setPlaceholderText("e.g., Damaged item, wrong size (press Enter to submit)");
    returnReasonInput->setMinimumHeight(60);
    returnReasonInput->setMaximumHeight(60);
    reasonLayout->addWidget(reasonLabel);
    reasonLayout->addWidget(returnReasonInput);

    // Install event filter so Enter key in reason field triggers Process Return
    returnReasonInput->installEventFilter(this);


    fieldsLayout->addLayout(itemIdLayout);
    fieldsLayout->addLayout(quantityLayout);
    fieldsLayout->addLayout(reasonLayout);

    processReturnButton = new QPushButton("Process Return");
    processReturnButton->setObjectName("dangerButton");
    processReturnButton->setMinimumHeight(45);
    processReturnButton->setCursor(Qt::PointingHandCursor);
    processReturnButton->setDefault(true); // Set as default for this tab

    formLayout->addWidget(formTitle);
    formLayout->addLayout(fieldsLayout);
    formLayout->addWidget(processReturnButton);
    formLayout->addStretch();

    // Right side - Returns History Table
    QFrame* tableFrame = new QFrame();
    tableFrame->setObjectName("cardFrame");

    QVBoxLayout* tableLayout = new QVBoxLayout(tableFrame);
    tableLayout->setContentsMargins(25, 25, 25, 25);
    tableLayout->setSpacing(15);

    QLabel* tableTitle = new QLabel("Return History");
    tableTitle->setObjectName("sectionTitle");

    returnsTable = new QTableWidget(); // Initialize the member variable here
    returnsTable->setObjectName("styledTable");
    returnsTable->setColumnCount(7);
    returnsTable->setHorizontalHeaderLabels({ "Date", "User", "Item ID", "Item Name", "Quantity", "Refund Amount", "Reason" });
    returnsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    returnsTable->setAlternatingRowColors(true);
    returnsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    returnsTable->verticalHeader()->setVisible(false);

    tableLayout->addWidget(tableTitle);
    tableLayout->addWidget(returnsTable);

    mainLayout->addWidget(formFrame);
    mainLayout->addWidget(tableFrame);

    // Connect signals
    connect(processReturnButton, &QPushButton::clicked, this, &MainWindow::processReturnItem);

    // Initial refresh of the returns table
    refreshReturnsTable(); // Call refresh after table is fully set up

    tabWidget->addTab(returnsTab, "↩️ Returns");
}

// NEW: Process Return Item
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == returnReasonInput && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Enter in reason field submits the return
            processReturnItem();
            return true; // Consume the event (don't insert newline)
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::processReturnItem() {
    QString itemId = returnItemIdInput->text().trimmed();
    QString quantityStr = returnQuantityInput->text().trimmed();
    QString reason = returnReasonInput->toPlainText().trimmed();

    bool quantityOk;
    int quantity = quantityStr.toInt(&quantityOk);

    if (itemId.isEmpty() || !quantityOk || quantity <= 0) {
        QMessageBox::warning(this, "Input Error", "Please enter a valid Item ID and a positive numeric quantity.");
        return;
    }

    // Ensure a user is logged in before processing a return that records against a user
    if (inventory.getCurrentUser().empty()) {
        QMessageBox::warning(this, "User Error", "Cannot process return: No user is currently logged in.");
        return;
    }

    if (inventory.processReturn(itemId.toStdString(), quantity, reason.toStdString())) {
        QMessageBox::information(this, "Return Processed",
            QString("Item '%1' (x%2) returned successfully. Inventory updated.").arg(itemId).arg(quantity));
        returnItemIdInput->clear();
        returnQuantityInput->clear();
        returnReasonInput->clear();
        refreshReturnsTable();
        // Also refresh the detailed sales/returns history table if it's open and active
        if (userHistoryTabWidget && userHistoryTabWidget->isVisible()) {
            QString currentUserName = userHistoryTabWidget->tabText(userHistoryTabWidget->currentIndex());
            refreshDetailedSalesReturnsTable(currentUserName.toStdString(), salesHistorySearchInput->text().trimmed(), salesHistoryStartDate->date(), salesHistoryEndDate->date());
        }
    }
    else {
        // More specific error messages could be provided by processReturn if it returned an enum/status code
        QMessageBox::warning(this, "Return Error", "Failed to process return. Item not found or an internal error occurred (e.g., trying to return more than exists, though this is not typical for returns).");
    }
}

// NEW: Refresh Returns Table
void MainWindow::refreshReturnsTable() {
    if (!returnsTable) { // Robustness: check if returnsTable is initialized
        qDebug() << "Error: returnsTable is null in refreshReturnsTable().";
        return;
    }

    returnsTable->setRowCount(0); // Clear existing rows to prevent memory leaks and ensure fresh data

    int row = 0;
    // Iterate through all users to get their return history
    for (const auto& username : inventory.getAllUsers()) {
        const auto& returnsForUser = inventory.getReturnsForUser(username); //
        for (const auto& ret : returnsForUser) {
            // Ensure the date string is in the expected format (yyyy-MM-dd)
            QDate returnDate = QDate::fromString(QString::fromStdString(ret.date), "yyyy-MM-dd"); //
            if (!returnDate.isValid()) { // Debugging: Check if date conversion was successful
                qDebug() << "Warning: Invalid date string encountered for return record for user " << QString::fromStdString(username) << ": " << QString::fromStdString(ret.date); //
                // Optionally, skip this row or set a default/error date string
                // For now, we will skip records with invalid dates to prevent potential crashes
                continue; // Skip this problematic record
            }

            returnsTable->insertRow(row);
            returnsTable->setItem(row, 0, new QTableWidgetItem(returnDate.toString("yyyy-MM-dd"))); // Use validated date
            returnsTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(username))); //
            returnsTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(ret.itemId))); //
            returnsTable->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(ret.itemName))); //
            returnsTable->setItem(row, 4, new QTableWidgetItem(QString::number(ret.quantity))); //
            returnsTable->setItem(row, 5, new QTableWidgetItem(QString("$%1").arg(ret.refundAmount, 0, 'f', 2))); //
            returnsTable->setItem(row, 6, new QTableWidgetItem(QString::fromStdString(ret.reason))); //
            row++;
        }
    }
    returnsTable->setColumnWidth(6, 150);
}

void MainWindow::refreshIndebtednessForCustomer(const std::string& customer) {
    auto table = indebtednessTables[customer];
    const auto& entries = inventory.getIndebtedness(customer); // Get entries from DB
    table->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(e.date)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(e.goodsDescription)));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(e.amountOwed, 'f', 2)));
        table->setItem(i, 3, new QTableWidgetItem(QString::number(e.amountPaid, 'f', 2))); // Display paid amount

        double remaining = e.getRemainingOwed();
        QTableWidgetItem* remainingItem = new QTableWidgetItem(QString::number(remaining, 'f', 2));
        remainingItem->setForeground(remaining > 0 ? Qt::red : Qt::darkGreen);
        table->setItem(i, 4, remainingItem); // Display remaining amount

        // Payment Details Column
        QTableWidgetItem* paymentDetailsItem = new QTableWidgetItem(e.paymentHistory.empty() ? "Click to view" : "View Payments");
        paymentDetailsItem->setTextAlignment(Qt::AlignCenter);
        paymentDetailsItem->setFlags(paymentDetailsItem->flags() & ~Qt::ItemIsEditable); // Make it non-editable
        table->setItem(i, 5, paymentDetailsItem);
    }

    // Update the summary card for this customer
    if (customerSummaryCards.count(customer)) {
        QFrame* summaryCard = customerSummaryCards[customer];
        QLabel* valueLabel = summaryCard->findChild<QLabel*>("valueLabel");
        if (valueLabel) {
            double newTotalOwed = inventory.getTotalOwedByCustomer(customer);
            valueLabel->setText(QString("LE%1").arg(newTotalOwed, 0, 'f', 2));
            summaryCard->setStyleSheet(QString(
                "background-color: %1;"
                "border-radius: 8px;"
                "padding: 15px;"
                "color: white;"
            ).arg(newTotalOwed > 0 ? "#e74c3c" : "#2ecc71"));
        }
    }
}

void MainWindow::refreshDeficienciesTable() {
    if (!deficienciesTable) return;

    // Ensure 4 columns (upgrade from old 3-column layout)
    if (deficienciesTable->columnCount() < 4) {
        deficienciesTable->setColumnCount(4);
        deficienciesTable->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Current Qty", "Threshold" });
    }

    deficienciesTable->setRowCount(0);

    const auto& lowItems = inventory.getItemsBelowThreshold();
    int row = 0;
    for (const auto& item : lowItems) {
        deficienciesTable->insertRow(row);
        deficienciesTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.id)));
        deficienciesTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(item.name)));
        deficienciesTable->setItem(row, 2, new QTableWidgetItem(QString::number(item.quantity)));
        deficienciesTable->setItem(row, 3, new QTableWidgetItem(QString::number(item.threshold)));

        for (int col = 0; col < 4; ++col) {
            deficienciesTable->item(row, col)->setBackground(QColor(255, 240, 240));
            deficienciesTable->item(row, col)->setForeground(Qt::darkRed);
        }
        row++;
    }
}
void MainWindow::searchItem() {
    QString query = searchInput->text().trimmed(); // Trim whitespace
    const auto& inv = inventory.getInventory(); // Get current inventory from DB

    searchResultsTable->setRowCount(0); // Clear previous results
    std::vector<Item> results;

    for (const auto& pair : inv) {
        const Item& item = pair.second;
        if (query.isEmpty() || QString::fromStdString(item.id).contains(query, Qt::CaseInsensitive) ||
            QString::fromStdString(item.name).contains(query, Qt::CaseInsensitive)) {
            results.push_back(item);
        }
    }

    searchResultsTable->setRowCount(results.size());

    int row = 0;
    for (const auto& item : results) {
        double totalSellValue = item.quantity * item.price;

        searchResultsTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.id)));
        searchResultsTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(item.name)));
        searchResultsTable->setItem(row, 2, new QTableWidgetItem(QString::number(item.quantity)));
        searchResultsTable->setItem(row, 3, new QTableWidgetItem(QString("LE%1").arg(item.price, 0, 'f', 2)));
        searchResultsTable->setItem(row, 4, new QTableWidgetItem(QString("LE%1").arg(item.boughtPrice, 0, 'f', 2)));
        searchResultsTable->setItem(row, 5, new QTableWidgetItem(QString("LE%1").arg(totalSellValue, 0, 'f', 2)));

        row++;
    }

    if (results.empty() && !query.isEmpty()) {
        QMessageBox::information(this, "Search Results", "No matching items found for your search query.");
    }

    // Hide suggestions when a full search is triggered
    searchSuggestionsList->hide();
    searchSuggestionsList->clear();
}

// ── Edit Inventory Item ───────────────────────────────────────────────────────
void MainWindow::editInventoryItem(const QString& itemId,
    QTableWidget* inventoryTableDialog,
    std::function<void()> refreshFn)
{
    // Look up the item from the inventory
    const auto& inv = inventory.getInventory();
    auto it = inv.find(itemId.toStdString());
    if (it == inv.end()) {
        QMessageBox::warning(this, "Not Found",
            QString("Item '%1' was not found in the inventory.").arg(itemId));
        return;
    }
    const Item& currentItem = it->second;

    // Build an edit dialog pre-filled with current values
    QDialog editDialog(this);
    editDialog.setWindowTitle(QString("Edit Item — %1").arg(itemId));
    editDialog.setMinimumWidth(380);
    editDialog.setStyleSheet("QDialog { background-color: #f5f5f5; }");

    QVBoxLayout* layout = new QVBoxLayout(&editDialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    QLabel* title = new QLabel(QString("Editing: <b>%1</b>").arg(itemId));
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(12);

    // Item Name
    QLineEdit* nameEdit = new QLineEdit(QString::fromStdString(currentItem.name));
    nameEdit->setObjectName("styledInput");
    form->addRow("Item Name:", nameEdit);

    // Quantity
    QLineEdit* qtyEdit = new QLineEdit(QString::number(currentItem.quantity));
    qtyEdit->setObjectName("styledInput");
    qtyEdit->setValidator(new QIntValidator(0, 9999999, &editDialog));
    form->addRow("Quantity:", qtyEdit);

    // Sell Price
    QLineEdit* priceEdit = new QLineEdit(QString::number(currentItem.price, 'f', 2));
    priceEdit->setObjectName("styledInput");
    priceEdit->setValidator(new QDoubleValidator(0.0, 9999999.99, 2, &editDialog));
    form->addRow("Sell Price (LE):", priceEdit);

    // Bought Price
    QLineEdit* boughtEdit = new QLineEdit(QString::number(currentItem.boughtPrice, 'f', 2));
    boughtEdit->setObjectName("styledInput");
    boughtEdit->setValidator(new QDoubleValidator(0.0, 9999999.99, 2, &editDialog));
    form->addRow("Bought Price (LE):", boughtEdit);

    // Low-stock Threshold
    QLineEdit* thresholdEdit = new QLineEdit(QString::number(currentItem.threshold));
    thresholdEdit->setObjectName("styledInput");
    thresholdEdit->setValidator(new QIntValidator(0, 99999, &editDialog));
    form->addRow("Low-Stock Threshold:", thresholdEdit);

    layout->addLayout(form);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* saveBtn = new QPushButton("💾  Save Changes");
    saveBtn->setObjectName("primaryButton");
    saveBtn->setMinimumHeight(42);
    saveBtn->setDefault(true);

    QPushButton* cancelBtn = new QPushButton("Cancel");
    cancelBtn->setObjectName("secondaryButton");
    cancelBtn->setMinimumHeight(42);
    cancelBtn->setAutoDefault(false);

    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, &editDialog, &QDialog::reject);

    connect(saveBtn, &QPushButton::clicked, this, [&]() {
        QString newName = nameEdit->text().trimmed();
        QString qtyStr = qtyEdit->text().trimmed();
        QString priceStr = priceEdit->text().trimmed();
        QString boughtStr = boughtEdit->text().trimmed();

        if (newName.isEmpty() || qtyStr.isEmpty() || priceStr.isEmpty() || boughtStr.isEmpty()) {
            QMessageBox::warning(&editDialog, "Validation Error", "All fields are required.");
            return;
        }

        bool okQ, okP, okB;
        int    newQty = qtyStr.toInt(&okQ);
        double newPrice = priceStr.toDouble(&okP);
        double newBought = boughtStr.toDouble(&okB);
        int    newThreshold = thresholdEdit->text().trimmed().isEmpty()
            ? 5
            : thresholdEdit->text().trimmed().toInt();

        if (!okQ || newQty < 0) {
            QMessageBox::warning(&editDialog, "Validation Error", "Quantity must be a non-negative integer.");
            return;
        }
        if (!okP || newPrice <= 0) {
            QMessageBox::warning(&editDialog, "Validation Error", "Sell price must be a positive number.");
            return;
        }
        if (!okB || newBought < 0) {
            QMessageBox::warning(&editDialog, "Validation Error", "Bought price must be a non-negative number.");
            return;
        }

        // Persist changes via a direct SQL UPDATE
        QSqlQuery q;
        q.prepare(
            "UPDATE items SET item_name = :item_name, quantity = :quantity, "
            "price = :price, bought_price = :bought_price, threshold = :threshold "
            "WHERE item_id = :item_id"
        );
        q.bindValue(":item_name", newName);
        q.bindValue(":quantity", newQty);
        q.bindValue(":price", newPrice);
        q.bindValue(":bought_price", newBought);
        q.bindValue(":threshold", newThreshold);
        q.bindValue(":item_id", itemId);

        if (!q.exec()) {
            QMessageBox::critical(&editDialog, "Database Error",
                "Failed to update item:\n" + q.lastError().text());
            return;
        }

        refreshFn();               // refresh the dialog table
        searchItem();              // refresh the main search tab
        refreshDeficienciesTable();

        refreshDeficienciesTable();
        QMessageBox::information(&editDialog, "Saved",
            QString("Item '%1' updated successfully.").arg(itemId));
        editDialog.accept();
        });

    editDialog.exec();
}

// ── Delete Inventory Item ─────────────────────────────────────────────────────
void MainWindow::deleteInventoryItem(const QString& itemId,
    QTableWidget* inventoryTableDialog,
    std::function<void()> refreshFn)
{
    // Confirm with the user before deleting
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete",
        QString("Are you sure you want to permanently delete item <b>%1</b> from the inventory?<br>"
            "<br><i>This action cannot be undone.</i>").arg(itemId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No   // default to No for safety
    );

    if (reply != QMessageBox::Yes)
        return;

    QSqlQuery q;
    q.prepare("DELETE FROM items WHERE item_id = :id");
    q.bindValue(":id", itemId);

    if (!q.exec()) {
        QMessageBox::critical(this, "Database Error",
            "Failed to delete item:\n" + q.lastError().text());
        return;
    }

    refreshFn();               // refresh the dialog table
    searchItem();              // refresh the main search tab
    refreshDeficienciesTable();

    QMessageBox::information(this, "Deleted",
        QString("Item '%1' has been removed from the inventory.").arg(itemId));
}
// ── Low-stock notification popup ─────────────────────────────────────────────
void MainWindow::checkAndNotifyLowStock() {
    // Show only once per login session (flag is reset in setCurrentUser)
    if (lowStockNotifiedThisSession_) return;
    lowStockNotifiedThisSession_ = true;

    const auto& lowItems = inventory.getItemsBelowThreshold();
    if (lowItems.empty()) {
        lowStockNotifiedThisSession_ = false; // nothing shown, allow retry next call
        return;
    }

    QDialog* alertDialog = new QDialog(this);
    alertDialog->setWindowTitle("Low Stock Alert");
    alertDialog->setFixedWidth(500);
    alertDialog->setAttribute(Qt::WA_DeleteOnClose);
    alertDialog->setStyleSheet(R"(
        QDialog { background-color: #f8f9fa; font-family: 'Segoe UI', Arial, sans-serif; }
        QLabel#alertBody {
            font-size: 13px; color: #2c3e50;
            background: white; border-radius: 10px;
            padding: 14px; border: 1px solid #e9ecef;
        }
        QPushButton#okAlertBtn {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #007bff, stop:1 #0056b3);
            color: white; border: none; border-radius: 8px;
            font-size: 13px; font-weight: 600;
            padding: 10px 28px; min-height: 38px;
        }
        QPushButton#okAlertBtn:hover {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #0056b3, stop:1 #004085);
        }
    )");

    QVBoxLayout* layout = new QVBoxLayout(alertDialog);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    // Red gradient header banner
    QFrame* banner = new QFrame();
    banner->setFixedHeight(72);
    banner->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #e74c3c, stop:1 #c0392b);"
        "border-top-left-radius: 4px; border-top-right-radius: 4px;"
    );
    QHBoxLayout* bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(20, 0, 20, 0);

    QLabel* warnIcon = new QLabel("⚠");
    warnIcon->setStyleSheet("font-size: 28px; color: #ffe066; background: transparent;");

    QLabel* bannerTitle = new QLabel(
        QString("%1 Item(s) Below Their Threshold!").arg(lowItems.size())
    );
    bannerTitle->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: white; background: transparent;"
    );

    bannerLayout->addWidget(warnIcon);
    bannerLayout->addSpacing(10);
    bannerLayout->addWidget(bannerTitle);
    bannerLayout->addStretch();
    layout->addWidget(banner);

    layout->addSpacing(14);

    // Table of low-stock items
    QTableWidget* alertTable = new QTableWidget((int)lowItems.size(), 4, alertDialog);
    alertTable->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Current Qty", "Threshold" });
    alertTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    alertTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    alertTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    alertTable->verticalHeader()->setVisible(false);
    alertTable->setAlternatingRowColors(true);
    alertTable->setObjectName("styledTable");
    alertTable->setMaximumHeight(220);

    for (int i = 0; i < (int)lowItems.size(); ++i) {
        const auto& item = lowItems[i];
        alertTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(item.id)));
        alertTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(item.name)));

        QTableWidgetItem* qtyItem = new QTableWidgetItem(QString::number(item.quantity));
        qtyItem->setForeground(QColor("#e74c3c"));
        qtyItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        alertTable->setItem(i, 2, qtyItem);

        QTableWidgetItem* thrItem = new QTableWidgetItem(QString::number(item.threshold));
        thrItem->setForeground(QColor("#6c757d"));
        alertTable->setItem(i, 3, thrItem);
    }

    QFrame* tableWrap = new QFrame();
    tableWrap->setContentsMargins(16, 0, 16, 0);
    QVBoxLayout* wl = new QVBoxLayout(tableWrap);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->addWidget(alertTable);
    layout->addWidget(tableWrap);

    layout->addSpacing(14);

    // Footer hint
    QLabel* hint = new QLabel(
        "  Please restock these items to avoid running out of stock."
    );
    hint->setStyleSheet("font-size: 12px; color: #6c757d; margin-left: 16px;");
    layout->addWidget(hint);

    layout->addSpacing(10);

    // OK button
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(16, 0, 16, 0);
    QPushButton* okBtn = new QPushButton("OK, Got It");
    okBtn->setObjectName("okAlertBtn");
    okBtn->setMinimumHeight(40);
    okBtn->setMinimumWidth(130);
    okBtn->setCursor(Qt::PointingHandCursor);
    connect(okBtn, &QPushButton::clicked, alertDialog, &QDialog::accept);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    alertDialog->exec();
}

// =============================================================================
// Resolve database absolute path (DB may be opened with a relative path)
// =============================================================================
static QString resolveDbPath() {
    QString dbPath = QSqlDatabase::database().databaseName();
    if (dbPath.isEmpty()) return QString();
    QFileInfo fi(dbPath);
    if (fi.isRelative()) {
        // Try app directory first, then current working directory
        QFileInfo fi2(QCoreApplication::applicationDirPath() + "/" + dbPath);
        if (fi2.exists()) return fi2.absoluteFilePath();
        QFileInfo fi3(QDir::currentPath() + "/" + dbPath);
        if (fi3.exists()) return fi3.absoluteFilePath();
        return fi.absoluteFilePath(); // best guess
    }
    return fi.absoluteFilePath();
}

// =============================================================================
// Automatic startup backup — runs before setupUI()
// =============================================================================
void MainWindow::performStartupBackup() {
    lastBackupStatus_ = QString();
    lastBackupOk_ = false;

    QString dbPath = resolveDbPath();
    if (dbPath.isEmpty() || !QFile::exists(dbPath)) {
        lastBackupStatus_ = "Database file not found — backup skipped.\n"
            "Path tried: " + QSqlDatabase::database().databaseName();
        return;
    }

    QFileInfo dbInfo(dbPath);
    QString backupDir = dbInfo.absolutePath() + "/backups";
    if (!QDir().mkpath(backupDir)) {
        lastBackupStatus_ = "Could not create backup folder:\n" + backupDir;
        return;
    }

    // Keep only the last 30 daily backups
    QDir bDir(backupDir);
    QStringList old = bDir.entryList(QStringList() << "backup_*.db",
        QDir::Files, QDir::Name);
    while (old.size() >= 30) bDir.remove(old.takeFirst());

    // One backup per calendar day
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QString backupPath = backupDir + "/backup_" + today + ".db";

    if (QFile::exists(backupPath)) {
        lastBackupStatus_ = "Already backed up today (" + today + ").\nFile: " + backupPath;
        lastBackupOk_ = true;
        return;
    }

    if (QFile::copy(dbPath, backupPath)) {
        lastBackupStatus_ = "Backup created successfully.\nFile: " + backupPath;
        lastBackupOk_ = true;
    }
    else {
        lastBackupStatus_ = "Copy failed — check folder permissions.\n"
            "Source: " + dbPath + "\nTarget: " + backupPath;
        lastBackupOk_ = false;
    }
}

// =============================================================================
// Show backup result after UI is ready (called via QTimer::singleShot)
// =============================================================================
void MainWindow::showBackupStartupResult() {
    // Update the small status label in the header
    if (backupStatusLabel_) {
        if (lastBackupOk_) {
            backupStatusLabel_->setText("Backup OK");
            backupStatusLabel_->setStyleSheet(
                "font-size:11px; color:white;"
                "background-color:rgba(40,167,69,0.80);"
                "border-radius:10px; padding:3px 10px;");
        }
        else {
            backupStatusLabel_->setText("Backup FAILED");
            backupStatusLabel_->setStyleSheet(
                "font-size:11px; color:white;"
                "background-color:rgba(220,53,69,0.90);"
                "border-radius:10px; padding:3px 10px;");
        }
        backupStatusLabel_->setToolTip(lastBackupStatus_);
    }

    // Show a small non-intrusive result dialog
    QDialog* toast = new QDialog(this);
    toast->setWindowTitle("Startup Backup Status");
    toast->setFixedWidth(430);
    toast->setAttribute(Qt::WA_DeleteOnClose);

    QString bannerColor = lastBackupOk_
        ? "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #28a745,stop:1 #1e7e34);"
        : "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #dc3545,stop:1 #c82333);";

    toast->setStyleSheet(
        "QDialog { background-color:#f8f9fa; font-family:'Segoe UI',Arial,sans-serif; }"
        "QPushButton#toastOk {"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #007bff,stop:1 #0056b3);"
        "  color:white; border:none; border-radius:8px;"
        "  font-size:13px; font-weight:600; padding:8px 24px; min-height:34px; }"
        "QPushButton#toastOk:hover { background:#0056b3; }"
    );

    QVBoxLayout* lay = new QVBoxLayout(toast);
    lay->setContentsMargins(0, 0, 0, 14);
    lay->setSpacing(0);

    // Coloured banner
    QFrame* banner = new QFrame();
    banner->setFixedHeight(58);
    banner->setStyleSheet(bannerColor +
        "border-top-left-radius:4px; border-top-right-radius:4px;");
    QHBoxLayout* bl = new QHBoxLayout(banner);
    bl->setContentsMargins(16, 0, 16, 0);
    QLabel* iconLbl = new QLabel(lastBackupOk_ ? "OK" : "FAIL");
    iconLbl->setStyleSheet(
        "font-size:15px; font-weight:bold; color:white; background:transparent;");
    QLabel* titleLbl = new QLabel("Automatic Startup Backup");
    titleLbl->setStyleSheet(
        "font-size:14px; font-weight:bold; color:white; background:transparent;");
    bl->addWidget(iconLbl);
    bl->addSpacing(10);
    bl->addWidget(titleLbl);
    bl->addStretch();
    lay->addWidget(banner);
    lay->addSpacing(10);

    // Detail message
    QLabel* msg = new QLabel(lastBackupStatus_);
    msg->setWordWrap(true);
    msg->setStyleSheet(
        "font-size:12px; color:#2c3e50; background:white;"
        "border-radius:8px; padding:12px; border:1px solid #e9ecef;"
        "margin-left:14px; margin-right:14px;");
    lay->addWidget(msg);
    lay->addSpacing(10);

    // OK button + auto-close after 5 seconds
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(14, 0, 14, 0);
    QPushButton* okBtn = new QPushButton("OK");
    okBtn->setObjectName("toastOk");
    okBtn->setMinimumHeight(34);
    okBtn->setMinimumWidth(80);
    okBtn->setCursor(Qt::PointingHandCursor);
    connect(okBtn, &QPushButton::clicked, toast, &QDialog::accept);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    QTimer::singleShot(5000, toast, &QDialog::accept); // auto-close after 5s
    toast->exec();

    // If backup failed, show an urgent warning after the toast closes
    if (!lastBackupOk_) {
        QMessageBox::warning(this, "Auto-Backup Failed",
            "The automatic backup could not be created.\n\n"
            "Reason:\n" + lastBackupStatus_ + "\n\n"
            "Please open the Backup Manager (the Backup button in the header)\n"
            "and create a manual backup now.");
    }
}

// =============================================================================
// Backup Manager Dialog
// =============================================================================
void MainWindow::showBackupManager() {
    QString dbPath = resolveDbPath();
    if (dbPath.isEmpty()) dbPath = QSqlDatabase::database().databaseName();
    QFileInfo dbInfo(dbPath);
    QString backupDir = dbInfo.absolutePath() + "/backups";
    QDir bDir(backupDir);

    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle("Database Backup Manager");
    dlg->setMinimumWidth(600);
    dlg->setMinimumHeight(440);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    dlg->setStyleSheet(
        "QDialog { background-color:#f8f9fa; font-family:'Segoe UI',Arial,sans-serif; }"
        "QPushButton#backupNowBtn {"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #28a745,stop:1 #1e7e34);"
        "  color:white; border:none; border-radius:8px;"
        "  font-size:13px; font-weight:600; padding:10px 20px; min-height:38px; }"
        "QPushButton#backupNowBtn:hover { background:#1e7e34; }"
        "QPushButton#restoreBtn {"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #007bff,stop:1 #0056b3);"
        "  color:white; border:none; border-radius:8px;"
        "  font-size:13px; font-weight:600; padding:10px 20px; min-height:38px; }"
        "QPushButton#restoreBtn:hover { background:#0056b3; }"
        "QPushButton#deleteBackupBtn {"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #dc3545,stop:1 #c82333);"
        "  color:white; border:none; border-radius:8px;"
        "  font-size:13px; font-weight:600; padding:10px 20px; min-height:38px; }"
        "QPushButton#deleteBackupBtn:hover { background:#c82333; }"
        "QPushButton#closeBackupBtn {"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #6c757d,stop:1 #545b62);"
        "  color:white; border:none; border-radius:8px;"
        "  font-size:13px; font-weight:600; padding:10px 20px; min-height:38px; }"
        "QPushButton#closeBackupBtn:hover { background:#545b62; }"
    );

    QVBoxLayout* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    // Header banner
    QFrame* banner = new QFrame();
    banner->setFixedHeight(72);
    banner->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #667eea,stop:1 #764ba2);"
        "border-top-left-radius:4px; border-top-right-radius:4px;");
    QHBoxLayout* bannerL = new QHBoxLayout(banner);
    bannerL->setContentsMargins(20, 0, 20, 0);
    QLabel* ico = new QLabel("DB");
    ico->setStyleSheet("font-size:18px; font-weight:bold; color:white; background:transparent;");
    QLabel* bannerTitle = new QLabel("Database Backup Manager");
    bannerTitle->setStyleSheet(
        "font-size:16px; font-weight:bold; color:white; background:transparent;");
    QLabel* dbPathLbl = new QLabel(dbPath);
    dbPathLbl->setStyleSheet(
        "font-size:11px; color:rgba(255,255,255,0.75); background:transparent;");
    QVBoxLayout* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    titleCol->addWidget(bannerTitle);
    titleCol->addWidget(dbPathLbl);
    bannerL->addWidget(ico);
    bannerL->addSpacing(10);
    bannerL->addLayout(titleCol);
    bannerL->addStretch();
    layout->addWidget(banner);
    layout->addSpacing(14);

    QLabel* folderLabel = new QLabel(
        QString("  Backup folder: <b>%1</b>").arg(backupDir));
    folderLabel->setTextFormat(Qt::RichText);
    folderLabel->setStyleSheet("font-size:12px; color:#495057; margin-left:16px;");
    layout->addWidget(folderLabel);
    layout->addSpacing(8);

    // Backup list table
    QTableWidget* backupTable = new QTableWidget();
    backupTable->setObjectName("styledTable");
    backupTable->setColumnCount(3);
    backupTable->setHorizontalHeaderLabels({ "Backup File", "Created", "Size" });
    backupTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    backupTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    backupTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    backupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    backupTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    backupTable->verticalHeader()->setVisible(false);
    backupTable->setAlternatingRowColors(true);

    auto populateBackups = [=]() {
        backupTable->setRowCount(0);
        QStringList files = bDir.entryList(QStringList() << "backup_*.db",
            QDir::Files, QDir::Name | QDir::Reversed);
        for (int i = 0; i < files.size(); ++i) {
            QFileInfo fi(backupDir + "/" + files[i]);
            backupTable->insertRow(i);
            backupTable->setItem(i, 0, new QTableWidgetItem(fi.fileName()));
            backupTable->setItem(i, 1, new QTableWidgetItem(
                fi.lastModified().toString("yyyy-MM-dd  hh:mm")));
            double kb = fi.size() / 1024.0;
            QString sz = kb < 1024
                ? QString("%1 KB").arg(kb, 0, 'f', 1)
                : QString("%1 MB").arg(kb / 1024.0, 0, 'f', 2);
            backupTable->setItem(i, 2, new QTableWidgetItem(sz));
            if (fi.fileName().contains(QDate::currentDate().toString("yyyy-MM-dd"))) {
                for (int col = 0; col < 3; ++col) {
                    backupTable->item(i, col)->setBackground(QColor(220, 248, 228));
                    backupTable->item(i, col)->setForeground(QColor(25, 135, 84));
                }
            }
        }
        if (backupTable->rowCount() == 0) {
            backupTable->insertRow(0);
            QTableWidgetItem* empty = new QTableWidgetItem(
                "No backups found — click Backup Now to create one.");
            empty->setForeground(QColor("#6c757d"));
            backupTable->setItem(0, 0, empty);
            backupTable->setSpan(0, 0, 1, 3);
        }
        };
    populateBackups();

    QFrame* tw = new QFrame();
    tw->setContentsMargins(16, 0, 16, 0);
    QVBoxLayout* twl = new QVBoxLayout(tw);
    twl->setContentsMargins(0, 0, 0, 0);
    twl->addWidget(backupTable);
    layout->addWidget(tw);
    layout->addSpacing(14);

    // Buttons
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(16, 0, 16, 0);
    btnRow->setSpacing(10);
    QPushButton* backupNowBtn = new QPushButton("Backup Now");
    backupNowBtn->setObjectName("backupNowBtn");   backupNowBtn->setMinimumHeight(40);
    QPushButton* restoreBtn = new QPushButton("Restore Selected");
    restoreBtn->setObjectName("restoreBtn");        restoreBtn->setMinimumHeight(40);
    QPushButton* deleteBackupBtn = new QPushButton("Delete Selected");
    deleteBackupBtn->setObjectName("deleteBackupBtn"); deleteBackupBtn->setMinimumHeight(40);
    QPushButton* closeBackupBtn = new QPushButton("Close");
    closeBackupBtn->setObjectName("closeBackupBtn"); closeBackupBtn->setMinimumHeight(40);
    btnRow->addWidget(backupNowBtn);
    btnRow->addWidget(restoreBtn);
    btnRow->addWidget(deleteBackupBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBackupBtn);
    layout->addLayout(btnRow);

    // Backup Now
    connect(backupNowBtn, &QPushButton::clicked, dlg, [=]() {
        QDir().mkpath(backupDir);
        QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString dest = backupDir + "/backup_" + ts + ".db";
        if (QFile::copy(dbPath, dest)) {
            QMessageBox::information(dlg, "Backup Created",
                "Backup saved:\n" + dest);
            populateBackups();
        }
        else {
            QMessageBox::critical(dlg, "Backup Failed",
                "Could not create backup.\nCheck folder permissions:\n" + backupDir);
        }
        });

    // Restore Selected
    connect(restoreBtn, &QPushButton::clicked, dlg, [=]() {
        int row = backupTable->currentRow();
        if (row < 0 || !backupTable->item(row, 0) ||
            backupTable->item(row, 0)->text().startsWith("No backups")) {
            QMessageBox::warning(dlg, "No Selection",
                "Please select a backup file from the list."); return;
        }
        QString sel = backupTable->item(row, 0)->text();
        QString src = backupDir + "/" + sel;
        if (!QFile::exists(src)) {
            QMessageBox::warning(dlg, "File Not Found",
                "Selected backup no longer exists."); return;
        }
        auto reply = QMessageBox::warning(dlg, "Confirm Restore",
            "This will REPLACE your current database with:\n\n" + sel +
            "\n\nThe app will close after restoring. Are you sure?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        QString conn = QSqlDatabase::database().connectionName();
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(conn);

        QString safety = dbPath + ".before_restore";
        QFile::remove(safety);
        QFile::copy(dbPath, safety);
        QFile::remove(dbPath);

        if (QFile::copy(src, dbPath)) {
            QMessageBox::information(dlg, "Restore OK",
                "Database restored.\nPlease restart the application.");
        }
        else {
            QFile::copy(safety, dbPath);
            QMessageBox::critical(dlg, "Restore Failed",
                "Restore failed. Original database kept.\nPlease restart the application.");
        }
        QApplication::quit();
        });

    // Delete Selected
    connect(deleteBackupBtn, &QPushButton::clicked, dlg, [=]() {
        int row = backupTable->currentRow();
        if (row < 0 || !backupTable->item(row, 0) ||
            backupTable->item(row, 0)->text().startsWith("No backups")) {
            QMessageBox::warning(dlg, "No Selection",
                "Please select a backup to delete."); return;
        }
        QString fn = backupTable->item(row, 0)->text();
        auto reply = QMessageBox::question(dlg, "Confirm Delete",
            "Delete backup:\n" + fn + "\n\nThis cannot be undone.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            QFile::remove(backupDir + "/" + fn);
            populateBackups();
        }
        });

    connect(closeBackupBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    dlg->exec();
}
