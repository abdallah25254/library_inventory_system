// mainwindow.cpp - REVISED with Enter key default button and auto-tabbing on scan
#include "mainwindow.h"
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
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUI();
    //initializeInventory(); // Call this to ensure some initial data is in the DB if it's empty
    updateUserDisplay();
}

void MainWindow::showSuppliersDashboard() {
    bool ok;
    QString password = QInputDialog::getText(this, "Admin Access",
        "Enter admin password:", QLineEdit::Password,
        "", &ok);

    if (ok && !password.isEmpty() && password == "25254amm") {
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
        if (password == "25254amm") {
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

    headerLayout->addWidget(logoLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
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


    headerLayout->addWidget(totalHistoryButton);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(suppliersButton);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(inventoryButton);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(logoutButton);


    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::handleLogout);
    connect(totalHistoryButton, &QPushButton::clicked, this, &MainWindow::showTotalSalesHistory);
    connect(suppliersButton, &QPushButton::clicked, this, &MainWindow::showSuppliersDashboard);
    connect(inventoryButton, &QPushButton::clicked, this, &MainWindow::showInventoryManagement);

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
    orderFormFrame->setMaximumHeight(350);

    QVBoxLayout* formLayout = new QVBoxLayout(orderFormFrame);
    formLayout->setContentsMargins(25, 25, 25, 25);
    formLayout->setSpacing(20);

    QLabel* formTitle = new QLabel("Add to Order");
    formTitle->setObjectName("sectionTitle");

    QVBoxLayout* orderFieldsLayout = new QVBoxLayout();
    orderFieldsLayout->setSpacing(15);

    QVBoxLayout* orderIdLayout = new QVBoxLayout();
    orderIdLayout->setSpacing(5);
    QLabel* orderIdLabel = new QLabel("Item ID");
    orderIdLabel->setObjectName("fieldLabel");
    orderItemIdInput = new QLineEdit();
    orderItemIdInput->setObjectName("styledInput");
    orderItemIdInput->setPlaceholderText("Enter item ID");
    orderIdLayout->addWidget(orderIdLabel);
    orderIdLayout->addWidget(orderItemIdInput);

    // NEW: Auto-tabbing for orderItemIdInput
    connect(orderItemIdInput, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.isEmpty()) return;
        // Check if the entered text is a valid item ID in the inventory
        // Now checks database
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM items WHERE item_id = :item_id");
        query.bindValue(":item_id", text);
        if (query.exec() && query.next() && query.value(0).toInt() > 0) {
            orderQuantityInput->setFocus(); // Move focus to quantity input
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


    orderTotalLabel = new QLabel("Total: LE0.00");
    orderTotalLabel->setObjectName("totalLabel");

    actionsLayout->addWidget(completeOrderButton);
    actionsLayout->addWidget(cancelOrderButton);
    actionsLayout->addWidget(deorderButton); // Add the new button
    actionsLayout->addStretch();
    actionsLayout->addWidget(orderTotalLabel);

    tableLayout->addWidget(orderTableTitle);
    tableLayout->addWidget(orderTable);
    tableLayout->addWidget(actionsFrame);

    mainLayout->addWidget(orderFormFrame);
    mainLayout->addWidget(orderTableFrame);

    connect(addToOrderButton, &QPushButton::clicked, this, &MainWindow::addItemToOrder);
    connect(completeOrderButton, &QPushButton::clicked, this, &MainWindow::completeCurrentOrder);
    connect(cancelOrderButton, &QPushButton::clicked, this, &MainWindow::cancelCurrentOrder);
    connect(deorderButton, &QPushButton::clicked, this, &MainWindow::onDeorderItemClicked); // Connect new button

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

    QLabel* tableTitle = new QLabel("Inventory Deficiencies (< 8 items)");
    tableTitle->setObjectName("sectionTitle");

    deficienciesTable = new QTableWidget(); // Initialize the member variable
    deficienciesTable->setObjectName("styledTable");
    deficienciesTable->setColumnCount(3); // Item ID, Item Name, Current Quantity
    deficienciesTable->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Current Quantity" });
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
        QMessageBox::information(this, "Order Completed",
            QString("Order completed successfully!\nTotal: %1")
            .arg(orderTotalLabel->text()));

        refreshOrderTable(); // Clears the current order display
        searchItem(); // Refresh search results (shows all if no search term)
        refreshDeficienciesTable(); // Refresh deficiencies
        // If the detailed history dashboard is open, refresh it as well.
        // This is complex as it would need access to its dialog,
        // so for now, it relies on re-opening the dashboard to see updates.
        // Or, if `refreshDetailedSalesReturnsTable` was a direct member, could call here.
        // For simplicity, we rely on the user to re-open Admin History.
    }
    else {
        QMessageBox::warning(this, "Order Error", "Failed to complete order. No user logged in or other issue.");
    }
}

void MainWindow::cancelCurrentOrder() {
    if (inventory.getCurrentOrder().empty()) {
        QMessageBox::warning(this, "Error", "No current order to cancel");
        return;
    }

    inventory.cancelOrder();
    refreshOrderTable(); // Clears the current order display
    searchItem(); // Refresh search results (shows all if no search term)
    refreshDeficienciesTable(); // Refresh deficiencies
    QMessageBox::information(this, "Order Cancelled", "Current order has been cancelled");
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
        // double totalBoughtValue = item.quantity * item.boughtPrice; // Not displayed in this table

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

    if (ok && !password.isEmpty() && password == "25254amm") {
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
        connect(itemIdInput, &QLineEdit::textChanged, this, [=](const QString& text) {
            if (text.isEmpty()) return;
            // For simplicity, just move to item name upon any text entry.
            // A more sophisticated check might be 'if (inventory.getInventory().count(text.toStdString()))'
            // and then decide whether to move to quantity or item name based on 'new item' vs 'existing item' flow.
            itemNameInput->setFocus(); // Move focus to item name input
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


        fieldsLayout->addLayout(idLayout);
        fieldsLayout->addLayout(nameLayout);
        fieldsLayout->addLayout(quantityLayout);
        fieldsLayout->addLayout(priceLayout);
        fieldsLayout->addLayout(boughtPriceLayout);

        addItemButton = new QPushButton("Add Item to Inventory");
        addItemButton->setObjectName("primaryButton");
        addItemButton->setMinimumHeight(45);
        addItemButton->setCursor(Qt::PointingHandCursor);
        addItemButton->setDefault(true); // Set as default for this dialog

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
        inventoryTableDialog->setColumnCount(6);
        inventoryTableDialog->setHorizontalHeaderLabels({ "Item ID", "Item Name", "Quantity", "Sell Price", "Bought Price", "Total Sell Value" });
        inventoryTableDialog->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        inventoryTableDialog->setAlternatingRowColors(true);
        inventoryTableDialog->setSelectionBehavior(QAbstractItemView::SelectRows);
        inventoryTableDialog->verticalHeader()->setVisible(false);

        // Populate the table
        auto refreshInventoryTable = [=]() {
            const auto& inv = inventory.getInventory(); // Get current inventory from DB
            inventoryTableDialog->setRowCount(inv.size());

            int row = 0;
            for (const auto& pair : inv) {
                const Item& item = pair.second;
                double totalSellValue = item.quantity * item.price;

                inventoryTableDialog->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.id)));
                inventoryTableDialog->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(item.name)));
                inventoryTableDialog->setItem(row, 2, new QTableWidgetItem(QString::number(item.quantity)));
                inventoryTableDialog->setItem(row, 3, new QTableWidgetItem(QString("LE%1").arg(item.price, 0, 'f', 2)));
                inventoryTableDialog->setItem(row, 4, new QTableWidgetItem(QString("LE%1").arg(item.boughtPrice, 0, 'f', 2)));
                inventoryTableDialog->setItem(row, 5, new QTableWidgetItem(QString("LE%1").arg(totalSellValue, 0, 'f', 2)));
                row++;
            }
            };

        // Initial table population
        refreshInventoryTable();

        tableLayout->addWidget(tableTitle);
        tableLayout->addWidget(inventoryTableDialog);

        // New: Buttons for total sell and bought money
        QHBoxLayout* totalsLayout = new QHBoxLayout();
        totalsLayout->setSpacing(10);
        totalsLayout->addStretch();

        QPushButton* showTotalSellMoneyButton = new QPushButton("Show Total Sell Money");
        showTotalSellMoneyButton->setObjectName("secondaryButton");
        showTotalSellMoneyButton->setMinimumHeight(40);
        showTotalSellMoneyButton->setCursor(Qt::PointingHandCursor);

        QPushButton* showTotalBoughtMoneyButton = new QPushButton("Show Total Bought Money");
        showTotalBoughtMoneyButton->setObjectName("secondaryButton");
        showTotalBoughtMoneyButton->setMinimumHeight(40);
        showTotalBoughtMoneyButton->setCursor(Qt::PointingHandCursor);

        totalsLayout->addWidget(showTotalSellMoneyButton);
        totalsLayout->addWidget(showTotalBoughtMoneyButton);
        tableLayout->addLayout(totalsLayout);

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

            inventory.addItem(id.toStdString(), name.toStdString(), quantity, price, boughtPrice);
            clearInventoryForm();
            refreshInventoryTable();
            refreshDeficienciesTable(); // Also refresh deficiencies if inventory changes
            QMessageBox::information(dialog, "Success", "Item added successfully!");
            });

        // Connect "Show Total Sell Money" button with password protection
        connect(showTotalSellMoneyButton, &QPushButton::clicked, this, [=, &dialog]() {
            bool authOk;
            QString adminPassword = QInputDialog::getText(dialog, "Admin Access",
                "Enter admin password to view total sell money:", QLineEdit::Password,
                "", &authOk);

            if (authOk && !adminPassword.isEmpty() && adminPassword == "25254amm") {
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

            if (authOk && !adminPassword.isEmpty() && adminPassword == "25254amm") {
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

    // NEW: Auto-tabbing for returnItemIdInput
    connect(returnItemIdInput, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.isEmpty()) return;
        // Check if the entered text is a valid item ID in the inventory
        // Now checks database
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM items WHERE item_id = :item_id");
        query.bindValue(":item_id", text);
        if (query.exec() && query.next() && query.value(0).toInt() > 0) {
            returnQuantityInput->setFocus(); // Move focus to quantity input
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

    QVBoxLayout* reasonLayout = new QVBoxLayout();
    reasonLayout->setSpacing(5);
    QLabel* reasonLabel = new QLabel("Reason for Return");
    reasonLabel->setObjectName("fieldLabel");
    returnReasonInput = new QTextEdit();
    returnReasonInput->setObjectName("styledInput");
    returnReasonInput->setPlaceholderText("e.g., Damaged item, wrong size");
    returnReasonInput->setMinimumHeight(60);
    reasonLayout->addWidget(reasonLabel);
    reasonLayout->addWidget(returnReasonInput);


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
void MainWindow::processReturnItem() {
    QString itemId = returnItemIdInput->text().trimmed();
    QString quantityStr = returnQuantityInput->text().trimmed();
    QString reason = returnReasonInput->toPlainText().trimmed();

    bool quantityOk;
    int quantity = quantityStr.toInt(&quantityOk);

    if (itemId.isEmpty() || !quantityOk || quantity <= 0 || reason.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter a valid Item ID, a positive numeric quantity, and a reason for return.");
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
    if (!deficienciesTable) return; // Add check for initialization

    deficienciesTable->setRowCount(0); // Clear existing rows

    const auto& inventoryItems = inventory.getInventory(); // Get the current inventory from DB
    int row = 0; //

    for (const auto& pair : inventoryItems) { //
        const Item& item = pair.second; //
        if (item.quantity <= 7) { // Check if quantity is 7 or less
            deficienciesTable->insertRow(row); //
            deficienciesTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.id))); //
            deficienciesTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(item.name))); //
            deficienciesTable->setItem(row, 2, new QTableWidgetItem(QString::number(item.quantity))); //

            // Optionally, highlight these rows to draw attention
            for (int col = 0; col < deficienciesTable->columnCount(); ++col) { //
                deficienciesTable->item(row, col)->setBackground(QColor(255, 240, 240)); // Light red background
                deficienciesTable->item(row, col)->setForeground(Qt::darkRed); // Dark red text
            }
            row++; //
        }
    }
}