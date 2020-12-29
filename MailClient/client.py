from socket import *
import base64
subject = "这是一封计算机网络测试邮件"
contenttype = "text/plain"
msg = "\r\n I love computer networks!"
endmsg = "\r\n.\r\n" 
# Choose a mail server (e.g. Google mail server) and call it mailserver
mailserver = "smtp.qq.com"
# Sender and reciever
# Fill in start
sender = '******'
senderMail = '******'
senderPassWord = '******'
recieverMail = '******'
# Fill in end

# Auth information (Encode with base64)
# Fill in start
authMail = base64.b64encode(senderMail.encode()) + b'\r\n'
authPassword = base64.b64encode(senderPassWord.encode()) + b'\r\n'
# Fill in end

# Create socket called clientSocket and establish a TCP connection with mailserver
#Fill in start
clientSocket = socket(AF_INET, SOCK_STREAM)
clientSocket.connect((mailserver, 587))
#Fill in end
recv = clientSocket.recv(1024).decode()
print(recv)
if recv[:3] != '220':
    print('220 reply not received from server.')

# Send HELO command and print server response.
heloCommand = 'HELO Alice\r\n'
clientSocket.send(heloCommand.encode())
recv1 = clientSocket.recv(1024).decode()
print(recv1)
if recv1[:3] != '250':
    print('250 reply not received from server.')

# login
while True:
    # 读取缓冲区的数据
    recv1 = clientSocket.recv(1024).decode()
    print(recv1)
    # login
    print('logining......')
    loginCommand = 'auth login\r\n'
    while True:
        clientSocket.send(loginCommand.encode())
        recv = clientSocket.recv(1024)
        recv = recv.decode()
        print(recv)
        if recv[:3] == '334':
            break
    # mail
    while True:
        clientSocket.send(authMail)
        recv = clientSocket.recv(1024)
        recv = recv.decode()
        print(recv)
        if recv[:3] == '334':
            break
    # password
    clientSocket.send(authPassword)
    recv = clientSocket.recv(1024)
    recv = recv.decode()
    print(recv)
    if recv[:3] == '235':
        break

# Send MAIL FROM command and print server response.
# Fill in start
mailFromCommand = 'Mail from:<'+ senderMail + '>\r\n'
while True:
    clientSocket.send(mailFromCommand.encode())
    recv = clientSocket.recv(1024)
    recv = recv.decode()
    print(recv)
    if recv[:3] == '250':
        break
# Fill in end

# Send RCPT TO command and print server response.
# Fill in start
RCPTToCommand = 'Rcpt to:<'+ recieverMail + '>\r\n'
while True:
    clientSocket.send(RCPTToCommand.encode())
    recv = clientSocket.recv(1024)
    recv = recv.decode()
    print(recv)
    if recv[:3] == '250':
        break
# Fill in end

# Send DATA command and print server response.
# Fill in start
dataCommand = 'data\r\n'
while True:
    clientSocket.send(dataCommand.encode())
    recv = clientSocket.recv(1024)
    recv = recv.decode()
    print(recv)
    if recv[:3] == '354':
        break
# Fill in end

# Send message data.
# Fill in start
fromCommand = 'from:'+ sender + '\r\n'
clientSocket.send(fromCommand.encode())
subjectCommand = 'subject:'+ subject + '\r\n'
clientSocket.send(subjectCommand.encode())
contenttypeCommand = 'contenttype:'+ subject + '\r\n'
clientSocket.send(contenttypeCommand.encode())
clientSocket.send(msg.encode())
# Fill in end

# Message ends with a single period.
# Fill in start
while True:
    clientSocket.send(endmsg.encode())
    recv = clientSocket.recv(1024)
    recv = recv.decode()
    print(recv)
    if recv[:3] == '250':
        break
# Fill in end

# Send QUIT command and get server response.
# Fill in start
quitCommand = 'quit\r\n'
while True:
    clientSocket.send(quitCommand.encode())
    recv = clientSocket.recv(1024)
    recv = recv.decode()
    print(recv)
    if recv[:3] == '221':
        break
# Fill in end
# Close connection
# Fill in start
clientSocket.close()
# Fill in end