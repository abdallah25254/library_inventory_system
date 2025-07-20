#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QPushButton>

class AddSupplierDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddSupplierDialog(QWidget* parent = nullptr);

    QString getSupplierName() const;
    QString getDate() const;
    double getTotalAmount() const;
    double getPaidAmount() const;

private:
    QLineEdit* supplierNameInput;
    QDateEdit* dateInput;
    QDoubleSpinBox* totalAmountInput;
    QDoubleSpinBox* paidAmountInput;
    QPushButton* addButton;
    QPushButton* cancelButton;
};
