Написать две программы, клиент и сервер. Сервер ожидает подключений множества клиентский программ, и передаёт текстовые сообщения между ними. Т.е. один клиент отправил строку, все остальные клиенты её получили и отобразили. 
Оба приложения взаимодействуют с пользователем через консоль.
Клиентская программа ожидает пользовательские команды. 
Пока она находится в режиме ожидания, выводит на консоль сообщения полученные от других клиентов. 
Команды вводятся через нажатия соответствующих клавиш:<br>

* "s" - send: ввод и отправка сообщения. После нажатия появляется приглашение на ввод текстового сообщения для отправки.
* "e" - exit: завершение работы программы-клиента

Передачу данных организовать по протоколу TCP/IP, через асинхронные вызовы (использовать boost::asio)
В каждой из программ может быть только один thread
Cобирать проект с помощью cmake

***

**Сборка:** <br>
 cd vbr_test <br>
 cmake . ; make install <br>

**Запуск:** <br>
 cd dist/bin <br>
 ./vbr_server --help <br>
 ./vbr_client --help <br>
 ./vbr_server <br>

Тестировалось на x86-64 Centos 7.3 и Debian 8.7 со стандартными для этих дистрибутивов версиями gcc и boost.
