#include "add_supplier_dialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>

AddSupplierDialog::AddSupplierDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Add Supplier Transaction");
    setMinimumSize(400, 300);
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QFormLayout* formLayout = new QFormLayout();

    supplierNameInput = new QLineEdit();
    supplierNameInput->setPlaceholderText("e.g., Ahmed Stationery");
    formLayout->addRow("Supplier Name:", supplierNameInput);

    dateInput = new QDateEdit(QDate::currentDate());
    dateInput->setCalendarPopup(true);
    formLayout->addRow("Transaction Date:", dateInput);

    totalAmountInput = new QDoubleSpinBox();
    totalAmountInput->setPrefix("$ ");
    totalAmountInput->setRange(0, 100000);
    totalAmountInput->setDecimals(2);
    formLayout->addRow("Total Amount:", totalAmountInput);

    paidAmountInput = new QDoubleSpinBox();
    paidAmountInput->setPrefix("$ ");
    paidAmountInput->setRange(0, 100000);
    paidAmountInput->setDecimals(2);
    formLayout->addRow("Paid Amount:", paidAmountInput);

    mainLayout->addLayout(formLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    addButton = new QPushButton("Add");
    cancelButton = new QPushButton("Cancel");
    buttonLayout->addStretch();
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(addButton, &QPushButton::clicked, this, &AddSupplierDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &AddSupplierDialog::reject);
}

QString AddSupplierDialog::getSupplierName() const {
    return supplierNameInput->text();
}

QString AddSupplierDialog::getDate() const {
    return dateInput->date().toString("yyyy-MM-dd");
}

double AddSupplierDialog::getTotalAmount() const {
    return totalAmountInput->value();
}

double AddSupplierDialog::getPaidAmount() const {
    return paidAmountInput->value();
}
