#include "library_project.h"
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <ctime>
#include <iomanip>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime> // For QDateTime to handle dates

LibraryInventory::LibraryInventory() {
    // Database connection is now managed externally (e.g., in main.cpp)
}

void LibraryInventory::addItem(const std::string& id, const std::string& name, int quantity, double price, double boughtPrice) {
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO items (item_id, item_name, quantity, price, bought_price) "
        "VALUES (:item_id, :item_name, :quantity, :price, :bought_price)");
    query.bindValue(":item_id", QString::fromStdString(id));
    query.bindValue(":item_name", QString::fromStdString(name));
    query.bindValue(":quantity", quantity);
    query.bindValue(":price", price);
    query.bindValue(":bought_price", boughtPrice);

    if (!query.exec()) {
        qDebug() << "Error adding/updating item:" << query.lastError().text();
    }
    else {
        qDebug() << "Item added/updated successfully:" << QString::fromStdString(id);
    }
}

bool LibraryInventory::addToOrder(const std::string& itemId, int quantity, double discountPercent) {
    QSqlQuery query;
    query.prepare("SELECT item_name, quantity, price FROM items WHERE item_id = :item_id");
    query.bindValue(":item_id", QString::fromStdString(itemId));

    if (!query.exec()) {
        qDebug() << "Error querying item for order:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false; // Item not found
    }

    QString itemName = query.value("item_name").toString();
    int currentInventoryQuantity = query.value("quantity").toInt();
    double unitPrice = query.value("price").toDouble();

    if (currentInventoryQuantity < quantity || quantity <= 0) {
        return false; // Insufficient quantity or invalid quantity requested
    }

    // Update inventory quantity in DB
    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE items SET quantity = quantity - :quantity WHERE item_id = :item_id");
    updateQuery.bindValue(":quantity", quantity);
    updateQuery.bindValue(":item_id", QString::fromStdString(itemId));
    if (!updateQuery.exec()) {
        qDebug() << "Error updating item quantity for order:" << updateQuery.lastError().text();
        return false;
    }

    // Add/update item in the in-memory current order
    bool foundInOrder = false;
    for (auto& orderItem : currentOrder_) {
        if (orderItem.itemId == itemId) {
            orderItem.quantity += quantity;
            orderItem.totalPrice = orderItem.quantity * orderItem.unitPrice * (1.0 - orderItem.discountPercent / 100.0);
            foundInOrder = true;
            break;
        }
    }

    if (!foundInOrder) {
        OrderItem newOrderItem;
        newOrderItem.itemId = itemId;
        newOrderItem.itemName = itemName.toStdString();
        newOrderItem.quantity = quantity;
        newOrderItem.unitPrice = unitPrice;
        newOrderItem.discountPercent = discountPercent;
        newOrderItem.totalPrice = quantity * unitPrice * (1.0 - discountPercent / 100.0);
        currentOrder_.push_back(newOrderItem);
    }
    return true;
}

bool LibraryInventory::completeOrder() {
    if (currentOrder_.empty()) {
        return false;
    }
    if (currentUser_.empty()) {
        return false; // No user logged in
    }

    QSqlDatabase::database().transaction(); // Start a transaction

    QSqlQuery salesQuery;
    salesQuery.prepare("INSERT INTO sales (sale_date, username, total_amount) VALUES (:sale_date, :username, :total_amount)");
    salesQuery.bindValue(":sale_date", QDate::currentDate().toString("yyyy-MM-dd"));
    salesQuery.bindValue(":username", QString::fromStdString(currentUser_));

    double totalOrderAmount = 0.0;
    for (const auto& item : currentOrder_) {
        totalOrderAmount += item.totalPrice;
    }
    salesQuery.bindValue(":total_amount", totalOrderAmount);

    if (!salesQuery.exec()) {
        qDebug() << "Error inserting into sales:" << salesQuery.lastError().text();
        QSqlDatabase::database().rollback(); // Rollback on error
        return false;
    }

    int saleNumber = salesQuery.lastInsertId().toInt(); // Get the auto-incremented sale_number

    QSqlQuery saleItemsQuery;
    saleItemsQuery.prepare("INSERT INTO sale_items (sale_number, item_id, item_name, unit_price, quantity, discount_percent, total_price) "
        "VALUES (:sale_number, :item_id, :item_name, :unit_price, :quantity, :discount_percent, :total_price)");

    for (const auto& item : currentOrder_) {
        saleItemsQuery.bindValue(":sale_number", saleNumber);
        saleItemsQuery.bindValue(":item_id", QString::fromStdString(item.itemId));
        saleItemsQuery.bindValue(":item_name", QString::fromStdString(item.itemName));
        saleItemsQuery.bindValue(":unit_price", item.unitPrice);
        saleItemsQuery.bindValue(":quantity", item.quantity);
        saleItemsQuery.bindValue(":discount_percent", item.discountPercent);
        saleItemsQuery.bindValue(":total_price", item.totalPrice);

        if (!saleItemsQuery.exec()) {
            qDebug() << "Error inserting into sale_items:" << saleItemsQuery.lastError().text();
            QSqlDatabase::database().rollback(); // Rollback on error
            return false;
        }
    }

    QSqlDatabase::database().commit(); // Commit the transaction
    currentOrder_.clear();
    return true;
}

void LibraryInventory::cancelOrder() {
    // Return items to inventory in DB if order is cancelled
    QSqlDatabase::database().transaction();
    QSqlQuery query;
    bool success = true;
    for (const auto& orderItem : currentOrder_) {
        query.prepare("UPDATE items SET quantity = quantity + :quantity WHERE item_id = :item_id");
        query.bindValue(":quantity", orderItem.quantity);
        query.bindValue(":item_id", QString::fromStdString(orderItem.itemId));
        if (!query.exec()) {
            qDebug() << "Error returning item quantity to inventory:" << query.lastError().text();
            success = false;
            break;
        }
    }
    if (success) {
        QSqlDatabase::database().commit();
    }
    else {
        QSqlDatabase::database().rollback();
    }
    currentOrder_.clear();
}

std::map<std::string, Item> LibraryInventory::getInventory() const {
    std::map<std::string, Item> inventoryMap;
    QSqlQuery query("SELECT item_id, item_name, quantity, price, bought_price FROM items");

    if (!query.exec()) {
        qDebug() << "Error getting inventory:" << query.lastError().text();
        return inventoryMap;
    }

    while (query.next()) {
        Item item;
        item.id = query.value("item_id").toString().toStdString();
        item.name = query.value("item_name").toString().toStdString();
        item.quantity = query.value("quantity").toInt();
        item.price = query.value("price").toDouble();
        item.boughtPrice = query.value("bought_price").toDouble();
        inventoryMap[item.id] = item;
    }
    return inventoryMap;
}

const std::vector<OrderItem>& LibraryInventory::getCurrentOrder() const {
    return currentOrder_;
}

std::vector<Sale> LibraryInventory::getSalesHistory(const std::string& username) const {
    std::vector<Sale> sales;
    QSqlQuery query;
    query.prepare("SELECT sale_number, sale_date, total_amount FROM sales WHERE username = :username ORDER BY sale_date ASC");
    query.bindValue(":username", QString::fromStdString(username));

    if (!query.exec()) {
        qDebug() << "Error getting sales history for user:" << query.lastError().text();
        return sales;
    }

    while (query.next()) {
        Sale sale;
        sale.saleNumber = query.value("sale_number").toInt();
        sale.date = query.value("sale_date").toString().toStdString();
        sale.totalAmount = query.value("total_amount").toDouble();

        // Fetch associated sale items
        QSqlQuery itemsQuery;
        itemsQuery.prepare("SELECT item_id, item_name, quantity, unit_price, discount_percent, total_price FROM sale_items WHERE sale_number = :sale_number");
        itemsQuery.bindValue(":sale_number", sale.saleNumber);
        if (itemsQuery.exec()) {
            while (itemsQuery.next()) {
                OrderItem item;
                item.itemId = itemsQuery.value("item_id").toString().toStdString();
                item.itemName = itemsQuery.value("item_name").toString().toStdString();
                item.quantity = itemsQuery.value("quantity").toInt();
                item.unitPrice = itemsQuery.value("unit_price").toDouble();
                item.discountPercent = itemsQuery.value("discount_percent").toDouble();
                item.totalPrice = itemsQuery.value("total_price").toDouble();
                sale.items.push_back(item);
            }
        }
        else {
            qDebug() << "Error fetching sale items for sale" << sale.saleNumber << ":" << itemsQuery.lastError().text();
        }
        sales.push_back(sale);
    }
    return sales;
}

std::vector<Sale> LibraryInventory::getAllSalesHistory() const {
    std::vector<Sale> sales;
    QSqlQuery query("SELECT sale_number, sale_date, username, total_amount FROM sales ORDER BY sale_date ASC");

    if (!query.exec()) {
        qDebug() << "Error getting all sales history:" << query.lastError().text();
        return sales;
    }

    while (query.next()) {
        Sale sale;
        sale.saleNumber = query.value("sale_number").toInt();
        sale.date = query.value("sale_date").toString().toStdString();
        sale.totalAmount = query.value("total_amount").toDouble();
        // The username is available here, but the Sale struct doesn't have a direct member for it.
        // If needed in the Sale struct, you'd add `std::string username;` to it.

        // Fetch associated sale items
        QSqlQuery itemsQuery;
        itemsQuery.prepare("SELECT item_id, item_name, quantity, unit_price, discount_percent, total_price FROM sale_items WHERE sale_number = :sale_number");
        itemsQuery.bindValue(":sale_number", sale.saleNumber);
        if (itemsQuery.exec()) {
            while (itemsQuery.next()) {
                OrderItem item;
                item.itemId = itemsQuery.value("item_id").toString().toStdString();
                item.itemName = itemsQuery.value("item_name").toString().toStdString();
                item.quantity = itemsQuery.value("quantity").toInt();
                item.unitPrice = itemsQuery.value("unit_price").toDouble();
                item.discountPercent = itemsQuery.value("discount_percent").toDouble();
                item.totalPrice = itemsQuery.value("total_price").toDouble();
                sale.items.push_back(item);
            }
        }
        else {
            qDebug() << "Error fetching sale items for sale" << sale.saleNumber << ":" << itemsQuery.lastError().text();
        }
        sales.push_back(sale);
    }
    return sales;
}


void LibraryInventory::setCurrentUser(const std::string& username) {
    currentUser_ = username;
}

std::string LibraryInventory::getCurrentUser() const {
    return currentUser_;
}

std::vector<std::string> LibraryInventory::getAllUsers() const {
    std::vector<std::string> users;
    QSqlQuery query("SELECT username FROM users");
    if (!query.exec()) {
        qDebug() << "Error getting all users:" << query.lastError().text();
        return users;
    }
    while (query.next()) {
        users.push_back(query.value("username").toString().toStdString());
    }
    return users;
}

void LibraryInventory::addSupplierTransaction(const std::string& date, const std::string& supplierName, double totalAmount, double paidAmount) {
    QSqlDatabase::database().transaction();
    QSqlQuery query;

    // Ensure supplier exists (or create it)
    query.prepare("INSERT OR IGNORE INTO suppliers (supplier_name) VALUES (:supplier_name)");
    query.bindValue(":supplier_name", QString::fromStdString(supplierName));
    if (!query.exec()) {
        qDebug() << "Error ensuring supplier exists:" << query.lastError().text();
        QSqlDatabase::database().rollback();
        return;
    }

    query.prepare("INSERT INTO supplier_transactions (transaction_date, supplier_name, total_amount, paid_amount) "
        "VALUES (:transaction_date, :supplier_name, :total_amount, :paid_amount)");
    query.bindValue(":transaction_date", QString::fromStdString(date));
    query.bindValue(":supplier_name", QString::fromStdString(supplierName));
    query.bindValue(":total_amount", totalAmount);
    query.bindValue(":paid_amount", paidAmount);

    if (!query.exec()) {
        qDebug() << "Error adding supplier transaction:" << query.lastError().text();
        QSqlDatabase::database().rollback();
    }
    else {
        QSqlDatabase::database().commit();
    }
}

std::vector<SupplierTransaction> LibraryInventory::getSupplierTransactionsForSupplier(const std::string& supplierName) const {
    std::vector<SupplierTransaction> transactions;
    QSqlQuery query;
    query.prepare("SELECT transaction_id, transaction_date, supplier_name, total_amount, paid_amount FROM supplier_transactions WHERE supplier_name = :supplier_name ORDER BY transaction_date ASC");
    query.bindValue(":supplier_name", QString::fromStdString(supplierName));

    if (!query.exec()) {
        qDebug() << "Error getting supplier transactions:" << query.lastError().text();
        return transactions;
    }

    while (query.next()) {
        SupplierTransaction tx;
        tx.transactionId = query.value("transaction_id").toInt();
        tx.date = query.value("transaction_date").toString().toStdString();
        tx.supplierName = query.value("supplier_name").toString().toStdString();
        tx.totalAmount = query.value("total_amount").toDouble();
        tx.paidAmount = query.value("paid_amount").toDouble();
        tx.paymentHistory = getSupplierPaymentHistoryForTransaction(tx.transactionId); // Fetch payments
        transactions.push_back(tx);
    }
    return transactions;
}

std::vector<std::string> LibraryInventory::getAllSuppliers() const {
    std::vector<std::string> suppliers;
    QSqlQuery query("SELECT supplier_name FROM suppliers ORDER BY supplier_name ASC");
    if (!query.exec()) {
        qDebug() << "Error getting all suppliers:" << query.lastError().text();
        return suppliers;
    }
    while (query.next()) {
        suppliers.push_back(query.value("supplier_name").toString().toStdString());
    }
    return suppliers;
}

double LibraryInventory::getTotalRemainingForSupplier(const std::string& supplierName) const {
    QSqlQuery query;
    query.prepare("SELECT SUM(total_amount - paid_amount) FROM supplier_transactions WHERE supplier_name = :supplier_name");
    query.bindValue(":supplier_name", QString::fromStdString(supplierName));
    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }
    qDebug() << "Error getting total remaining for supplier:" << query.lastError().text();
    return 0.0;
}

void LibraryInventory::addSupplierPayment(const std::string& supplierName, const std::string& date, double paymentAmount) {
    QSqlDatabase::database().transaction();
    QSqlQuery selectTransactionsQuery;
    selectTransactionsQuery.prepare("SELECT transaction_id, (total_amount - paid_amount) AS remaining FROM supplier_transactions WHERE supplier_name = :supplier_name AND (total_amount - paid_amount) > 0 ORDER BY transaction_date ASC");
    selectTransactionsQuery.bindValue(":supplier_name", QString::fromStdString(supplierName));

    if (!selectTransactionsQuery.exec()) {
        qDebug() << "Error selecting transactions for payment:" << selectTransactionsQuery.lastError().text();
        QSqlDatabase::database().rollback();
        return;
    }

    double remainingPayment = paymentAmount;
    while (selectTransactionsQuery.next() && remainingPayment > 0) {
        int transactionId = selectTransactionsQuery.value("transaction_id").toInt();
        double remainingInTx = selectTransactionsQuery.value("remaining").toDouble();
        double amountToPay = std::min(remainingInTx, remainingPayment);

        QSqlQuery updateTxQuery;
        updateTxQuery.prepare("UPDATE supplier_transactions SET paid_amount = paid_amount + :amount WHERE transaction_id = :transaction_id");
        updateTxQuery.bindValue(":amount", amountToPay);
        updateTxQuery.bindValue(":transaction_id", transactionId);
        if (!updateTxQuery.exec()) {
            qDebug() << "Error updating transaction paid amount:" << updateTxQuery.lastError().text();
            QSqlDatabase::database().rollback();
            return;
        }

        QSqlQuery insertPaymentQuery;
        insertPaymentQuery.prepare("INSERT INTO supplier_payments (transaction_id, payment_date, amount) VALUES (:transaction_id, :payment_date, :amount)");
        insertPaymentQuery.bindValue(":transaction_id", transactionId);
        insertPaymentQuery.bindValue(":payment_date", QString::fromStdString(date));
        insertPaymentQuery.bindValue(":amount", amountToPay);
        if (!insertPaymentQuery.exec()) {
            qDebug() << "Error inserting supplier payment record:" << insertPaymentQuery.lastError().text();
            QSqlDatabase::database().rollback();
            return;
        }
        remainingPayment -= amountToPay;
    }

    QSqlDatabase::database().commit();
}

std::vector<PaymentRecord> LibraryInventory::getSupplierPaymentHistoryForTransaction(int transactionId) const {
    std::vector<PaymentRecord> payments;
    QSqlQuery query;
    query.prepare("SELECT payment_date, amount FROM supplier_payments WHERE transaction_id = :transaction_id ORDER BY payment_date ASC");
    query.bindValue(":transaction_id", transactionId);

    if (!query.exec()) {
        qDebug() << "Error getting supplier payment history:" << query.lastError().text();
        return payments;
    }
    while (query.next()) {
        payments.push_back({ query.value("payment_date").toString().toStdString(), query.value("amount").toDouble() });
    }
    return payments;
}

void LibraryInventory::addIndebtedness(const std::string& customer, const IndebtednessEntry& entry) {
    QSqlDatabase::database().transaction();
    QSqlQuery query;

    // Ensure customer exists (or create it)
    query.prepare("INSERT OR IGNORE INTO customers (customer_name) VALUES (:customer_name)");
    query.bindValue(":customer_name", QString::fromStdString(customer));
    if (!query.exec()) {
        qDebug() << "Error ensuring customer exists:" << query.lastError().text();
        QSqlDatabase::database().rollback();
        return;
    }

    query.prepare("INSERT INTO indebtedness_entries (customer_name, dept_date, goods_description, amount_owed, amount_paid) "
        "VALUES (:customer_name, :dept_date, :goods_description, :amount_owed, :amount_paid)");
    query.bindValue(":customer_name", QString::fromStdString(customer));
    query.bindValue(":dept_date", QString::fromStdString(entry.date));
    query.bindValue(":goods_description", QString::fromStdString(entry.goodsDescription));
    query.bindValue(":amount_owed", entry.amountOwed);
    query.bindValue(":amount_paid", entry.amountPaid); // Initial paid amount (usually 0)

    if (!query.exec()) {
        qDebug() << "Error adding indebtedness entry:" << query.lastError().text();
        QSqlDatabase::database().rollback();
    }
    else {
        QSqlDatabase::database().commit();
    }
}

std::vector<IndebtednessEntry> LibraryInventory::getIndebtedness(const std::string& customer) const {
    std::vector<IndebtednessEntry> entries;
    QSqlQuery query;
    query.prepare("SELECT dept_id, dept_date, customer_name, goods_description, amount_owed, amount_paid FROM indebtedness_entries WHERE customer_name = :customer_name ORDER BY dept_date ASC");
    query.bindValue(":customer_name", QString::fromStdString(customer));

    if (!query.exec()) {
        qDebug() << "Error getting indebtedness entries:" << query.lastError().text();
        return entries;
    }

    while (query.next()) {
        IndebtednessEntry entry;
        entry.debtId = query.value("dept_id").toInt();
        entry.date = query.value("dept_date").toString().toStdString();
        entry.customerName = query.value("customer_name").toString().toStdString();
        entry.goodsDescription = query.value("goods_description").toString().toStdString();
        entry.amountOwed = query.value("amount_owed").toDouble();
        entry.amountPaid = query.value("amount_paid").toDouble();
        entry.paymentHistory = getIndebtednessPaymentHistoryForEntry(entry.debtId); // Fetch payments
        entries.push_back(entry);
    }
    return entries;
}

bool LibraryInventory::addIndebtednessPayment(const std::string& customerName, const std::string& date, double paymentAmount, int debtId) {
    QSqlDatabase::database().transaction();
    QSqlQuery query;

    // Update the specific debt entry first
    query.prepare("UPDATE indebtedness_entries SET amount_paid = amount_paid + :amount WHERE dept_id = :debt_id AND customer_name = :customer_name");
    query.bindValue(":amount", paymentAmount);
    query.bindValue(":debt_id", debtId);
    query.bindValue(":customer_name", QString::fromStdString(customerName));

    if (!query.exec()) {
        qDebug() << "Error updating indebtedness entry for payment:" << query.lastError().text();
        QSqlDatabase::database().rollback();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        qDebug() << "No matching debt entry found for payment. Debt ID:" << debtId << "Customer:" << QString::fromStdString(customerName);
        QSqlDatabase::database().rollback();
        return false;
    }

    // Insert the payment record
    query.prepare("INSERT INTO indebtedness_payments (debt_id, payment_date, amount) VALUES (:debt_id, :payment_date, :amount)");
    query.bindValue(":debt_id", debtId);
    query.bindValue(":payment_date", QString::fromStdString(date));
    query.bindValue(":amount", paymentAmount);

    if (!query.exec()) {
        qDebug() << "Error inserting indebtedness payment record:" << query.lastError().text();
        QSqlDatabase::database().rollback();
        return false;
    }

    QSqlDatabase::database().commit();
    return true;
}

std::vector<std::string> LibraryInventory::getAllCustomersWithIndebtedness() const {
    std::vector<std::string> customers;
    QSqlQuery query("SELECT DISTINCT customer_name FROM customers ORDER BY customer_name ASC");
    if (!query.exec()) {
        qDebug() << "Error getting all customers with indebtedness:" << query.lastError().text();
        return customers;
    }
    while (query.next()) {
        customers.push_back(query.value("customer_name").toString().toStdString());
    }
    return customers;
}

double LibraryInventory::getTotalOwedByCustomer(const std::string& customerName) const {
    QSqlQuery query;
    query.prepare("SELECT SUM(amount_owed - amount_paid) FROM indebtedness_entries WHERE customer_name = :customer_name");
    query.bindValue(":customer_name", QString::fromStdString(customerName));
    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }
    qDebug() << "Error getting total owed by customer:" << query.lastError().text();
    return 0.0;
}

std::vector<PaymentRecord> LibraryInventory::getIndebtednessPaymentHistoryForEntry(int debtId) const {
    std::vector<PaymentRecord> payments;
    QSqlQuery query;
    query.prepare("SELECT payment_date, amount FROM indebtedness_payments WHERE debt_id = :debt_id ORDER BY payment_date ASC");
    query.bindValue(":debt_id", debtId);

    if (!query.exec()) {
        qDebug() << "Error getting indebtedness payment history:" << query.lastError().text();
        return payments;
    }
    while (query.next()) {
        payments.push_back({ query.value("payment_date").toString().toStdString(), query.value("amount").toDouble() });
    }
    return payments;
}


double LibraryInventory::getTotalInventorySellValue() const {
    QSqlQuery query("SELECT SUM(quantity * price) FROM items");
    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }
    qDebug() << "Error getting total inventory sell value:" << query.lastError().text();
    return 0.0;
}

double LibraryInventory::getTotalInventoryBoughtValue() const {
    QSqlQuery query("SELECT SUM(quantity * bought_price) FROM items");
    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }
    qDebug() << "Error getting total inventory bought value:" << query.lastError().text();
    return 0.0;
}

bool LibraryInventory::removeItemFromOrder(const std::string& itemId) {
    bool removed = false;
    // Find the item in the in-memory current order
    auto it = currentOrder_.begin();
    while (it != currentOrder_.end()) {
        if (it->itemId == itemId) {
            // Return quantity to main inventory in DB
            QSqlQuery updateQuery;
            updateQuery.prepare("UPDATE items SET quantity = quantity + :quantity WHERE item_id = :item_id");
            updateQuery.bindValue(":quantity", it->quantity);
            updateQuery.bindValue(":item_id", QString::fromStdString(itemId));
            if (!updateQuery.exec()) {
                qDebug() << "Error returning quantity to inventory on deorder:" << updateQuery.lastError().text();
                return false; // Failed to update DB, so don't remove from in-memory order
            }
            it = currentOrder_.erase(it); // Remove from in-memory order
            removed = true;
            break;
        }
        else {
            ++it;
        }
    }
    return removed;
}

bool LibraryInventory::processReturn(const std::string& itemId, int quantity, const std::string& reason) {
    QSqlDatabase::database().transaction();
    QSqlQuery query;

    // Check if item exists and get its details
    query.prepare("SELECT item_name, price FROM items WHERE item_id = :item_id");
    query.bindValue(":item_id", QString::fromStdString(itemId));
    if (!query.exec() || !query.next()) {
        qDebug() << "Item not found for return:" << QString::fromStdString(itemId);
        QSqlDatabase::database().rollback();
        return false;
    }
    QString itemName = query.value("item_name").toString();
    double itemPrice = query.value("price").toDouble();

    // Add quantity back to inventory in DB
    QSqlQuery updateInventoryQuery;
    updateInventoryQuery.prepare("UPDATE items SET quantity = quantity + :quantity WHERE item_id = :item_id");
    updateInventoryQuery.bindValue(":quantity", quantity);
    updateInventoryQuery.bindValue(":item_id", QString::fromStdString(itemId));
    if (!updateInventoryQuery.exec()) {
        qDebug() << "Error updating inventory for return:" << updateInventoryQuery.lastError().text();
        QSqlDatabase::database().rollback();
        return false;
    }

    // Create a return record in DB
    QSqlQuery insertReturnQuery;
    insertReturnQuery.prepare("INSERT INTO returns_history (return_date, username, item_id, item_name, quantity, refund_amount, reason) "
        "VALUES (:return_date, :username, :item_id, :item_name, :quantity, :refund_amount, :reason)");
    insertReturnQuery.bindValue(":return_date", QDate::currentDate().toString("yyyy-MM-dd"));
    insertReturnQuery.bindValue(":username", QString::fromStdString(currentUser_));
    insertReturnQuery.bindValue(":item_id", QString::fromStdString(itemId));
    insertReturnQuery.bindValue(":item_name", itemName);
    insertReturnQuery.bindValue(":quantity", quantity);
    insertReturnQuery.bindValue(":refund_amount", quantity * itemPrice); // Refund based on sell price
    insertReturnQuery.bindValue(":reason", QString::fromStdString(reason));

    if (!insertReturnQuery.exec()) {
        qDebug() << "Error inserting return record:" << insertReturnQuery.lastError().text();
        QSqlDatabase::database().rollback();
        return false;
    }

    QSqlDatabase::database().commit();
    return true;
}

std::vector<ReturnRecord> LibraryInventory::getReturnsForUser(const std::string& username) const {
    std::vector<ReturnRecord> returns;
    QSqlQuery query;
    query.prepare("SELECT return_id, return_date, item_id, item_name, quantity, refund_amount, reason FROM returns_history WHERE username = :username ORDER BY return_date ASC");
    query.bindValue(":username", QString::fromStdString(username));

    if (!query.exec()) {
        qDebug() << "Error getting returns for user:" << query.lastError().text();
        return returns;
    }

    while (query.next()) {
        ReturnRecord record;
        record.returnId = query.value("return_id").toInt();
        record.date = query.value("return_date").toString().toStdString();
        record.itemId = query.value("item_id").toString().toStdString();
        record.itemName = query.value("item_name").toString().toStdString();
        record.quantity = query.value("quantity").toInt();
        record.refundAmount = query.value("refund_amount").toDouble();
        record.reason = query.value("reason").toString().toStdString();
        returns.push_back(record);
    }
    return returns;
}

std::vector<ReturnRecord> LibraryInventory::getAllReturnsHistory() const {
    std::vector<ReturnRecord> returns;
    QSqlQuery query("SELECT return_id, return_date, username, item_id, item_name, quantity, refund_amount, reason FROM returns_history ORDER BY return_date ASC");

    if (!query.exec()) {
        qDebug() << "Error getting all returns history:" << query.lastError().text();
        return returns;
    }

    while (query.next()) {
        ReturnRecord record;
        record.returnId = query.value("return_id").toInt();
        record.date = query.value("return_date").toString().toStdString();
        // The username is available here, but the ReturnRecord struct doesn't have a direct member for it.
        // If needed in the ReturnRecord struct, you'd add `std::string username;` to it.
        record.itemId = query.value("item_id").toString().toStdString();
        record.itemName = query.value("item_name").toString().toStdString();
        record.quantity = query.value("quantity").toInt();
        record.refundAmount = query.value("refund_amount").toDouble();
        record.reason = query.value("reason").toString().toStdString();
        returns.push_back(record);
    }
    return returns;
}