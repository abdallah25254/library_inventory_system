// mainwindow.h - REVISED based on dynamic user tabs
#pragma once
#include <QDateEdit>
#include <QMainWindow>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QTextEdit>
#include <QTimer>
#include <QKeyEvent>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include "library_project.h" // Include your LibraryInventory header

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    //void initializeInventory(); // Still useful for initial data population if DB is empty
    void setCurrentUser(const std::string& username);
    void updateUserDisplay();
signals:
    void logoutRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:

    void setupDeficienciesTab(); // NEW
    void refreshDeficienciesTable(); // NEW
    void checkAndNotifyLowStock(); // NEW: popup alert for items below threshold
    void addItemToInventory(); // This slot might become less relevant if adding items from DB always
    void addItemToOrder();
    void completeCurrentOrder();
    void cancelCurrentOrder();
    void printReceipt(int saleNumber); // NEW: Print receipt function
    void searchItem();
    void updateOrderTotal();
    void applyModernStyling();
    void addShadowEffects();
    void handleLogout();
    void showTotalSalesHistory(); // This will be heavily modified
    void showSuppliersDashboard();
    void addSupplierTransactionFromForm();
    void addSupplierPayment(const std::string& supplierName, QDateEdit* paymentDateInput, QLineEdit* paymentInput, QFrame* summaryCard, QTableWidget* table); // Added QDateEdit
    void showPaymentHistory(const std::string& supplierName, int transactionIndex);
    void showInventoryManagement();
    void editInventoryItem(const QString& itemId, QTableWidget* inventoryTableDialog,
        std::function<void()> refreshFn);
    void deleteInventoryItem(const QString& itemId, QTableWidget* inventoryTableDialog,
        std::function<void()> refreshFn);

    void onSearchInputTextChanged(const QString& text);
    void onSuggestionItemSelected(QListWidgetItem* item);
    void onDeorderItemClicked();

    void setupReturnsTab();
    void processReturnItem();
    void refreshReturnsTable();

    void handleAddIndebtedness();
    void refreshIndebtednessForCustomer(const std::string& customer);
    void addIndebtednessPaymentToCustomer(const std::string& customerName, QDateEdit* paymentDateInput, QLineEdit* paymentInput, QFrame* summaryCard, QTableWidget* table);
    void showIndebtednessPaymentHistory(const std::string& customerName, int transactionIndex); // Now requires `debtId` from `IndebtednessEntry` struct

    // NEW: Slots for Sales History (Detailed Tab) Search and Filter
    void searchSalesReturnsHistory(); // Slot to apply search and date filters
    // This slot will now take the username as an argument to refresh a specific user's tab
    void refreshDetailedSalesReturnsTable(const std::string& username, const QString& searchTerm = "", const QDate& startDate = QDate(), const QDate& endDate = QDate());
    // Slot to handle tab changes in the detailed history section
    void onUserHistoryTabChanged(int index);


private:
    QLabel* userLabel;
    LibraryInventory inventory; // Your main inventory logic class
    QTabWidget* tabWidget;

    // Inventory Management Dialog elements
    QLineEdit* itemIdInput;
    QLineEdit* itemNameInput;
    QLineEdit* itemQuantityInput;
    QLineEdit* itemPriceInput;
    QLineEdit* boughtPriceInput;
    QLineEdit* itemThresholdInput;  // NEW: per-item low-stock threshold
    QPushButton* addItemButton;

    // Order Tab elements
    QTableWidget* orderTable;
    QLineEdit* orderItemIdInput;
    QListWidget* orderItemSuggestionsList;   // name-search dropdown
    QLineEdit* orderQuantityInput;
    QLineEdit* orderDiscountInput;
    QPushButton* addToOrderButton;
    QPushButton* completeOrderButton;
    QPushButton* cancelOrderButton;
    QPushButton* deorderButton;
    QPushButton* printReceiptButton; // NEW: Print receipt button
    QLabel* orderTotalLabel;

    // Search Tab elements
    QLineEdit* searchInput;
    QPushButton* searchButton;
    QTableWidget* searchResultsTable;
    QListWidget* searchSuggestionsList;

    // Main window header buttons
    QPushButton* logoutButton;
    QPushButton* totalHistoryButton;
    QPushButton* suppliersButton;
    QPushButton* inventoryButton;
    QPushButton* backupButton;

    // Supplier Dashboard elements
    QTabWidget* supplierTabWidget;
    std::map<std::string, QTableWidget*> supplierTables; // Map to hold tables for each supplier tab
    QLineEdit* supplierNameInput;
    QDateEdit* supplierDateInput;
    QLineEdit* totalAmountInput;
    QLineEdit* paidAmountInput;
    QPushButton* submitSupplierButton;

    // Indebtedness Tab elements
    QTabWidget* indebtednessTab;
    std::map<std::string, QTableWidget*> indebtednessTables; // Map to hold tables for each customer tab
    std::map<std::string, QFrame*> customerSummaryCards; // Map to hold summary cards for each customer

    QLineEdit* debtCustomerInput;
    QDateEdit* debtDateInput;
    QLineEdit* debtAmountInput;
    QLineEdit* debtGoodsInput;
    QPushButton* debtSubmitButton;

    // Returns Tab elements
    QTableWidget* returnsTable;
    QLineEdit* returnItemIdInput;
    QLineEdit* returnQuantityInput;
    QTextEdit* returnReasonInput;
    QPushButton* processReturnButton;

    // This member is unused after modification, can be removed if not needed elsewhere
    QDateEdit* indebtednessPaymentDateInput;

    // NEW: Members for Sales History (Detailed Tab) - GLOBAL FILTER CONTROLS
    QLineEdit* salesHistorySearchInput;
    QDateEdit* salesHistoryStartDate;
    QDateEdit* salesHistoryEndDate;
    QPushButton* salesHistoryApplyFilterButton;

    // NEW: QTabWidget for user-specific history tabs within the detailed view
    QTabWidget* userHistoryTabWidget;
    // NEW: Map to hold QTableWidget pointers for each user's history tab
    std::map<std::string, QTableWidget*> userHistoryTables;

    // For deficiencies tab
    QTableWidget* deficienciesTable;
    QTimer* deficienciesRefreshTimer;
    bool    lowStockNotifiedThisSession_ = false; // shows popup only once per login
    QString lastBackupStatus_;          // set by performStartupBackup()
    bool    lastBackupOk_ = false;          // true = backup succeeded
    QLabel* backupStatusLabel_ = nullptr;   // small pill label in header


    // Backup
    void performStartupBackup();
    void showBackupManager();
    void showBackupStartupResult();   // shows result toast after UI ready

    // Helper functions
    QFrame* createSummaryCard(const QString& title, const QString& value, const QString& color);
    void refreshSupplierTable(const std::string& supplierName, QTableWidget* table); // Helper to refresh a single supplier tab's table
    void setupIndebtednessTab();
    void setupUI();
    void setupOrderTab();
    void setupSearchTab();
    void refreshOrderTable(); // Refreshes the in-memory order display
    void clearOrderForm();
    void clearInventoryForm(); // Used in Inventory Management dialog
    void addSupplierTransactionDialog(); // This dialog might be removed/integrated differently
    bool hasDiscounts(const std::vector<OrderItem>& items); // Still applies to in-memory OrderItems
};
