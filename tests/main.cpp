#include <gtest/gtest.h>
#include <QCoreApplication>

int main(int argc, char* argv[])
{
    // QSqlDatabase 需要 QCoreApplication 才能運作
    QCoreApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
