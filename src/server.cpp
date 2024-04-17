#include "server.h"
#include "ui_server.h"

server::server(QWidget *parent) : // конструктор
    QWidget(parent),
    ui(new Ui::server)
{
    ui->setupUi(this);

    this->setWindowTitle("Режим сервера");
    this->setWindowIcon(QIcon("D:/Qt Projects/tcp/server_icon.png"));
    this->setFixedSize(643,601);

    ui->lineEdit_name->setPlaceholderText("Ваше имя здесь");

    color = Qt::red;
    ui->label_color->setStyleSheet(QString("background-color: %1").arg(color.name()));

    TcpServer = nullptr;

    ui->frame->setEnabled(0);

    QList<QHostAddress> ip_list = QNetworkInterface::allAddresses();

    ui->comboBox_ip->setEditable(true);
    ui->comboBox_ip->lineEdit()->setReadOnly(true);
    ui->comboBox_ip->lineEdit()->setAlignment(Qt::AlignCenter);

    for (int i = 0; i<ip_list.size();i++)
    {
        if (ip_list[i].protocol() == QAbstractSocket::IPv4Protocol)
            ui->comboBox_ip->addItem(ip_list[i].toString());
        ui->comboBox_ip->setItemData(ui->comboBox_ip->count()-1, Qt::AlignCenter, Qt::TextAlignmentRole);
    }

    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget_ban->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

server::~server() // деструктор
{
    if(TcpServer!=nullptr)
        TcpServer->deleteLater();
    delete ui;
}

void server::on_pushButton_connect_clicked() // создание сервер
{
    if (!server_connected)
    {
        if(ui->lineEdit_name->text().count("  ") or ui->lineEdit_name->text().size()<=0)
        {
            QMessageBox::information(this,"Внимание","Проверьте имя");
            return;
        }
        int port = ui->spinBox_port->value();

        if(TcpServer!=nullptr)
            TcpServer->deleteLater();
        TcpServer=nullptr;
        TcpServer=new QTcpServer(this);

        if(TcpServer->listen(QHostAddress::Any, quintptr(port)))
        {
            connect(TcpServer, &QTcpServer::newConnection, this, &server::new_connection);

            server_connected = true;
            ui->pushButton_connect->setText("Закрыть чат");

            ui->frame_2->setEnabled(0);
            ui->frame->setEnabled(1);

            nicks.insert(server_id, ui->lineEdit_name->text());
        }
        else
        {
            QMessageBox::warning(this,"Внимание","Порт занят");
            nicks.clear();
            return;
        }
    }
    else
    {
        if(Sockets.size()>0)
        {
            QMessageBox::information(this,"Внимание","Чат кирдык");

            for (QTcpSocket* socket : qAsConst(Sockets))
            {
                socket->close();
                socket->deleteLater();
                socket=nullptr;
            }

            Sockets.clear();

            TcpServer->close();
            TcpServer->deleteLater();
            TcpServer=nullptr;

            ui->frame_2->setEnabled(1);
            ui->frame->setEnabled(0);
            ui->chatEdit->clear();

            server_connected = false;
            ui->pushButton_connect->setText("Открыть чат");
        }

        else
        {
            TcpServer->close();
            if(TcpServer!=nullptr)
                TcpServer->deleteLater();
            TcpServer=nullptr;

            ui->frame_2->setEnabled(1);
            ui->frame->setEnabled(0);
            ui->chatEdit->clear();

            server_connected = false;
            ui->pushButton_connect->setText("Открыть чат");
        }
        nicks.clear();
    }
}

void server::new_connection() // новое соединение
{
    QTcpSocket *TcpSocket;
    TcpSocket=new QTcpSocket(this);
    TcpSocket=TcpServer->nextPendingConnection();
    QString ip_ckeck = static_cast<QHostAddress>(TcpSocket->peerAddress().toIPv4Address()).toString();
    if(ban_list.contains(ip_ckeck))
    {
        QByteArray Data = "✉⊘тут вам не рады";
        TcpSocket->write(Data.data(), Data.size());
        TcpSocket->waitForBytesWritten(3000); //исправление проблемы с уведомлением
        TcpSocket->close();

        message->close();
        message->set_warn("В дверь стучится нарушитель!");
        message->show();

        return;
    }
    else if(Sockets.size()<11)
    {
        QHostAddress tmp_adr = static_cast<QHostAddress>(TcpSocket->peerAddress().toIPv4Address());

        message->close();
        message->set_warn("В чат вошёл новенький");
        message -> show();

        nicks.insert(TcpSocket, "✖");

        connect(TcpSocket, &QTcpSocket::readyRead, this, &server::readData);
        connect(TcpSocket, &QTcpSocket::disconnected, this, &server::disconnected);

        Sockets.push_back(TcpSocket);
        ui->tableWidget->setRowCount(Sockets.size());

        int indx = Sockets.size()-1;

        QTableWidgetItem *item;
        item = new QTableWidgetItem;
        ui->tableWidget->setItem(indx, 1,item);
        ui->tableWidget->item(indx,1)->setText(tmp_adr.toString());

        QPushButton * pb = new QPushButton(this);
        pb->setText("бан");
        pb->setProperty("row",indx);
        connect(pb,SIGNAL(clicked()),this,SLOT(del_connection()));
        ui->tableWidget->setCellWidget(indx,2,pb);

        m_clients.insert(TcpSocket, QTime::currentTime());
    }
    else
    {
        QByteArray Data = "Чат переполнен";
        TcpSocket->write(Data.data(), Data.size());
        TcpSocket->close();

        message->close();
        message->set_warn("Попытки войти");
        message -> show();
        return;
    }
}

void server::del_connection() // блокировка
{
    QPushButton * pb = qobject_cast<QPushButton *>(sender());
    if (pb!=nullptr)
    {
        int indx=(pb->property("row").toInt());

        QTcpSocket *client = Sockets[indx];

        client_disc = true;

        QString ban_ip = ui->tableWidget->item(indx,1)->text();
        ban_list.append(ban_ip);

        {
            QHostAddress tmp_adr = static_cast<QHostAddress>(client->peerAddress().toIPv4Address());
            ui->tableWidget_ban->setRowCount(ban_list.size());

            int ban_indx = ban_list.size()-1;

            QTableWidgetItem *item;
            item = new QTableWidgetItem;
            ui->tableWidget_ban->setItem(ban_indx,0,item);
            ui->tableWidget_ban->item(ban_indx,0)->setText(tmp_adr.toString());

            QPushButton * pb = new QPushButton(this);
            pb->setText("Разбан");
            pb->setProperty("row",ban_indx);
            connect(pb,SIGNAL(clicked()),this,SLOT(antiBan()));
            ui->tableWidget_ban->setCellWidget(ban_indx,1,pb);
        }

        QByteArray Data = "✉⊘Вас исключили";
        client->write(Data.data(), Data.size());

        nicks.remove(client);

        Sockets.removeOne(client);
        client->close();

        for (QTcpSocket* socket : qAsConst(Sockets))
        {
            if (static_cast<QHostAddress>(socket->peerAddress().toIPv4Address()).toString() == ban_ip)
            {
                socket->write(Data.data(), Data.size());
                nicks.remove(socket);

                Sockets.removeOne(socket);
                socket->close();
            }
        }
    }
}

void server::antiBan() // разбан
{
    QPushButton * pb = qobject_cast<QPushButton *>(sender());
    if (pb!=nullptr)
    {
        int indx=(pb->property("row").toInt());

        ban_list.removeAt(indx);
        ui->tableWidget_ban->removeRow(indx);
    }
}

void server::readData() // читаем сообщение
{
    QByteArray Data;
    QTcpSocket *sender = qobject_cast<QTcpSocket*>(QObject::sender());

    while (sender->bytesAvailable() > 0) //реализация блочного чтения данных
        Data.append(sender->read(256));

    QString str = QString::fromUtf8(Data);
    QStringList str_list=str.split("⊘", QString::KeepEmptyParts);

    if (checkForSpam(sender) or Data.size()>=500)
    {
        client_disc = true;

        QString ban_ip = ui->tableWidget->item(Sockets.indexOf(sender),1)->text();
        ban_list.append(ban_ip);

        {
            QHostAddress tmp_adr = static_cast<QHostAddress>(sender->peerAddress().toIPv4Address());
            ui->tableWidget_ban->setRowCount(ban_list.size());

            int ban_indx = ban_list.size()-1;

            QTableWidgetItem *item;
            item = new QTableWidgetItem;
            ui->tableWidget_ban->setItem(ban_indx,0,item);
            ui->tableWidget_ban->item(ban_indx,0)->setText(tmp_adr.toString());

            QPushButton * pb = new QPushButton(this);
            pb->setText("разбан");
            pb->setProperty("row",ban_indx);
            connect(pb,SIGNAL(clicked()),this,SLOT(antiBan()));
            ui->tableWidget_ban->setCellWidget(ban_indx,1,pb);
        }

        Data.clear();

        Data = "✉⊘вас исключили";
        sender->write(Data.data(), Data.size());

        nicks.remove(sender);

        Sockets.removeOne(sender);
        sender->close();

        for (QTcpSocket* socket : qAsConst(Sockets))
        {
            if (static_cast<QHostAddress>(socket->peerAddress().toIPv4Address()).toString() == ban_ip)
            {
                Data.clear();

                Data = "✉⊘вас исключили";
                socket->write(Data.data(), Data.size());

                nicks.remove(socket);

                Sockets.removeOne(socket);
                socket->close();
            }
        }

        return;
    }

    else if (str_list[0] == "✉")
    {
        int k = 0;
        for (QTcpSocket* check : qAsConst(Sockets))
        {
            if (nicks[check] == str_list[1])
            {
                client_disc = true;
                Data.clear();
                Data = "✉⊘это имя занято";
                sender->write(Data.data(), Data.size());

                nicks.remove(sender);
                Sockets.removeOne(sender);
                sender->close();
                k++;
            }
        }

        if(k==0)
        {
            QTableWidgetItem *name = new QTableWidgetItem;
            name->setText(str_list[1]);
            ui->tableWidget->setItem(Sockets.indexOf(sender),0,name);
            nicks[sender] = str_list[1];

            onlineList();
        }
        return;
    }

    for (QTcpSocket *client : qAsConst(Sockets)) {
        if (client != sender) {
            client->write(Data, Data.size());
        }
    }

    ui->chatEdit->appendHtml("<span style=\"color:" + str_list[1] + "\">" + "<b>" + str_list[0] + "</b>"\
            + "<span style=\"color:" + str_list[1] + "\">" + " : " + str_list[2] + "</span>");
}

void server::onlineList() // обновление списка онлайна
{
    QByteArray list = "∑";

    for (int i = 0; i<Sockets.size(); i++)
    {
        QHostAddress adr = static_cast<QHostAddress>(Sockets[i]->peerAddress().toIPv4Address());
        ui->tableWidget->item(i,0)->setText(nicks[Sockets[i]]);
        ui->tableWidget->item(i,1)->setText(adr.toString());
        list.append("⊘" + nicks[Sockets[i]]);
    }

    for (QTcpSocket *client : qAsConst(Sockets)) {
        client->write(list);
    }
}

void server::disconnected() // отключение
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(QObject::sender());

    if(client_disc)
    {
        message->close();
        message->set_warn("Собеседник отключён");
        message -> show();

        ui->tableWidget->setRowCount(Sockets.size());
        client_disc = false;
        onlineList();
    }
    else
    {
        if (server_connected)
        {
            message->close();
            message->set_warn("Собеседник ушёл");
            message -> show();
        }

        nicks.remove(client);

        Sockets.removeOne(client);
        client->deleteLater();

        ui->tableWidget->setRowCount(Sockets.size());
        onlineList();
    }
}

void server::on_lineEdit_message_returnPressed() // отправка сообщения
{
    if(Sockets.size()>0 and ui->lineEdit_message->text().size()>0)
    {
        //char *data;
        int size;
        QString name = ui->lineEdit_name->text();

        QString color_str = color.name(QColor::HexArgb);

        QString text = ui->lineEdit_message->text().toHtmlEscaped();

        QByteArray Data = (name+"⊘"+color_str+"⊘"+ui->lineEdit_message->text()).toUtf8();

        size = Data.size();
        //data = Data.data();

        for (QTcpSocket *client : qAsConst(Sockets)) {
            client->write(Data, size);
        }

        ui->chatEdit->appendHtml("<span style=\"color:" + color_str + "\">" + "<b>" + name + "</b>" \
                                 + "<span style=\"color:" + color_str + "\">" + " : " + text + "</span>");
        ui->lineEdit_message->clear();
    }
}

void server::on_pushButton_name_color_clicked() // смена цвета имени
{
    color = QColorDialog::getColor(color,this);
    ui->label_color->setStyleSheet(QString("background-color: %1").arg(color.name()));
}

bool server::checkForSpam(QTcpSocket *clientSocket) // проверка на спам
{
    int maxMessagesPerSecond = 5;
    int maxMessagesPerMinute = 30;

    QTime currentTime = QTime::currentTime();
    QTime lastMessageTime = m_clients.value(clientSocket);

    int secondsSinceLastMessage = lastMessageTime.secsTo(currentTime);
    int messagesSinceLastMessage = m_messagesPerSecond.value(clientSocket, 0);

    if (secondsSinceLastMessage > 1)
    {
        m_messagesPerSecond.insert(clientSocket, 1);
        m_clients.insert(clientSocket, currentTime);
    }
    else
    {
        m_messagesPerSecond.insert(clientSocket, messagesSinceLastMessage + 1);
        m_clients.insert(clientSocket, currentTime);
    }

    if (messagesSinceLastMessage > maxMessagesPerSecond || (secondsSinceLastMessage <= 60 && messagesSinceLastMessage > maxMessagesPerMinute))
        return true;

    return false;
}

void server::closeEvent(QCloseEvent *event) // событие при закрытии окна
{
    message->close();
    event->accept();
}
