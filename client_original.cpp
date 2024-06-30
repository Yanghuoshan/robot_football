// -*-c++-*-

/***************************************************************************
 client.cpp  -  A basic client that connects to
 the server
 -------------------
 begin                : 27-DEC-2001
 copyright            : (C) 2001 by The RoboCup Soccer Server
 Maintenance Group.
 email                : sserver-admin@lists.sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LGPL as published by the Free Software  *
 *   Foundation; either version 3 of the License, or (at your option) any  *
 *   later version.                                                        *
 *                                                                         *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H  //��������  C++Ԥ������
#include "config.h"
#endif

#include "compress.h"

#include <rcssbase/net/socketstreambuf.hpp> //rcssbase����ʵ���������ӡ�������ȡ�����л���һЩͨ�õĹ��ܣ��Ƿ������server �ĵײ��
#include <rcssbase/net/udpsocket.hpp> //hpp,��ʵ�ʾ��ǽ�.cpp��ʵ�ִ������.hͷ�ļ����У�������ʵ�ֶ�������ͬһ�ļ�
#include <rcssbase/gzip/gzstream.hpp>

#ifdef HAVE_SSTREAM // ��C++�������ַ�������һ����sstream�ж��壬��һ����strstream�ж���
#include <sstream> //�����ַ���
#else//C++������ostringstream��istringstream��stringstream�������࣬Ҫʹ�����Ǵ�������ͱ������sstream.hͷ�ļ�
#include <strstream> //�ַ��������� ��c����
#endif
#include <iostream> //���������
#include <cerrno> // errno �Ǽ�¼ϵͳ�����һ�δ������  ��errno.h�ж���  errno��ͬ��ֵ��ʾ��ͬ�ĺ���
#include <csignal> // �����˳���ִ��ʱ��δ���ͬ���ź�
#include <cstdio> //cstdio�ǽ�stdio.h��������C++ͷ�ļ�����ʽ��ʾ������stdio.h��C��׼�������е�ͷ�ļ����ṩ���������ֵ��������������
#include <cstdlib> //cstdlib��C++�����һ�����ú����⣬ �ȼ���C�е�<stdlib.h>,stdlib.h�����ṩһЩ��������ų�����
#include <cstring> //<cstring>��C��׼��ͷ�ļ�<string.h>��C++��׼��汾  ������strcmp��strchr��strstr�Ȳ���
#include <string.h> // C�汾ͷ�ļ� ��Ӧ����char*���ַ���������

#ifdef __CYGWIN__
// cygwin is not win32
#elif defined(_WIN32) || defined(__WIN32__) || defined (WIN32)
#  define RCSS_WIN
#  include <winsock2.h> // ����ϵͳ���û�ʹ�õ����֮�����ڽ�����һ���ӿڣ�������ܾ����޸������ϵͳ��ȷ��ͨѶ������
#endif

#ifndef RCSS_WIN
#  include <unistd.h> // �� C++ ��������������ṩ�Բ���ϵͳ API �ķ��ʹ��ܵ�ͷ�ļ�������
#  include <sys/select.h> // select��һ�������������λ��ͷ�ļ�#include <sys/select.h> ���ú������ڼ����ļ��������ı仯���������д�����쳣��
#  include <sys/time.h> // �������ĸ��������͡�������͸��ֲ������ں�ʱ��ĺ���
#  include <sys/types.h> // ����ϵͳ�������� �����˺ܶ�����
#endif

int iPlayerId = 0;
int currentCycle = 0;
int lastCycle = 0;
int kickWait = 0;
int turnToSeeGoal = 0;
int iSide = 0;//1:left;2:right

class coord//������
{
    public:
    double x,y;

    coord(double X=0,double Y=0)
	{
		x=X;
		y=Y;
	}
	coord operator -(coord v1)
	{
		coord v;
		v.x=x-v1.x;
		v.y=y-v1.y;
		return v;
	}
	coord operator +(coord v1)
	{
		coord v;
		v.x=x+v1.x;
		v.y=y+v1.y;
		return v;
	}
};
coord initpos[11]={coord(-51,0),coord(-25,-25),coord(-25,-8),coord(-25,8),coord(-25,25),coord(-5,-9),coord(-9.5,3),coord(-5,9),coord(-5,-15),coord(-8.5,-3),coord(-5,15)};


//�������Ϊֱ������
void poly2vector(double dLen, double dAngle, double &dX, double &dY) {

}

class Client {
private:
	rcss::net::Addr M_dest;  // 1)global scope(ȫ��������������÷���::name)
	rcss::net::UDPSocket M_socket;  // 2)class scope(��������������÷�(class::name)
	rcss::net::SocketStreamBuf * M_socket_buf; // 3)namespace scope(�����ռ�������������÷�(namespace::name)
	rcss::gz::gzstreambuf * M_gz_buf;
	std::ostream * M_transport;
	int M_comp_level;
	bool M_clean_cycle;

#ifdef HAVE_LIBZ
	Decompressor M_decomp;
#endif

	Client(); // ���캯��
	Client(const Client &);
	Client & operator=(const Client &); //���������


	void init(char *msg);// ��ʼ��
    void hear(char *msg);            // hear����֪����ģʽ
    void sense(char *msg);           // sense_body
    void see(char *msg);             // see��������־�����ߡ�����Ա����Ϣ
    void updateINFO();               // ������Ϣ
    bool turn(double angle);         // ת��һ���Ƕ�
    bool gotoPos(double x, double y); // ����ǰ��(x,y)
    bool ballINField();              // ���Ƿ����Լ��Ļ��Χ��
    bool ballINPenalty();            // ���Ƿ��ڼ���������
    bool ballINOppopenalty();        // ���Ƿ��ڶԷ�������
    bool canGoal();                  // �ܷ����ţ����������ŽǶ�
    int ballWay();
    void init_pos();

public: // ���캯����ʼ���б���һ��ð�ſ�ʼ���������Զ��ŷָ������ݳ�Ա�б�ÿ�����ݳ�Ա�����һ�����������еĳ�ʼ��ʽ��
	Client(const std::string & server, const int port) :
		M_dest(port), M_socket(), M_socket_buf(NULL), M_gz_buf(NULL),
				M_transport(NULL), M_comp_level(-1), M_clean_cycle(true) {
		M_dest.setHost(server);
		open(); // ��
		bind(); // ��

		M_socket_buf->setEndPoint(M_dest); // ָ����ʽṹ������ĳ�Ա������ʹ�� -> �����
	}

	virtual ~Client() { // ��������
		close(); // �ر�
	}

	bool sendCmd(char *command) {
		int len;
		len = strlen(command) + 1; // strlen c++�ַ�����������
		printf("command:%s\n", command);
		M_transport->write(command, len);
		M_transport->flush();
		if (!M_transport->good()) {
			printf("error send socket\n");
			return false;
		}
		return true;
	}

	void run() {
		char command[20];
		if (iSide == 1) {
			if (iPlayerId == 1) {
				sprintf(command, "(init team1 (goalie))");
			}
			else {
				sprintf(command, "(init team1)");
			}
		} else if (iSide == 2) {
			if (iPlayerId == 1) {
				sprintf(command, "(init team2 (goalie))");
			}
			else {
				sprintf(command, "(init team2)");
			}
		} else {
			return;
		}
		if (!sendCmd(command))
			return;
		if (iPlayerId == 1) {
			sprintf(command, "(move -45 0)");
		}
		else {
			sprintf(command, "(move -3 0)");
		}
		if (!sendCmd(command))
			return;
		messageLoop(); // ����  ��Ϣѭ�� �����С�����
	}

private:

	int open() {
		if (M_socket.open()) {
			if (M_socket.setNonBlocking() < 0) {
				std::cerr << __FILE__ << ": " << __LINE__
						<< ": Error setting socket non-blocking: " << strerror(
						errno) << std::endl;
				M_socket.close();
				return -1;
			}
		} else {
			std::cerr << __FILE__ << ": " << __LINE__
					<< ": Error opening socket: " << strerror(errno)
					<< std::endl;
			M_socket.close();
			return -1;
		}

		M_socket_buf = new rcss::net::SocketStreamBuf(M_socket); // new ��̬�ڴ�
		M_transport = new std::ostream(M_socket_buf);
		return 0;
	}

	bool bind() {
		if (!M_socket.bind(rcss::net::Addr())) { // rcss::net::Addr() �����޲ι��캯������һ�������ٰѶ��󴫸�����
			std::cerr << __FILE__ << ": " << __LINE__
					<< ": Error connecting socket" << std::endl;
			M_socket.close();
			return false;
		}
		return true;
	}

	void close() {
		M_socket.close();

		if (M_transport) {
			delete M_transport;
			M_transport = NULL;
		}

		if (M_gz_buf) {
			delete M_gz_buf;
			M_gz_buf = NULL;
		}

		if (M_socket_buf) {
			delete M_socket_buf;
			M_socket_buf = NULL;
		}
	}

	int setCompression(int level) {
#ifdef HAVE_LIBZ
		if ( level >= 0 )
		{
			if ( ! M_gz_buf )
			{
				M_gz_buf = new rcss::gz::gzstreambuf( *M_socket_buf );
			}
			M_gz_buf->setLevel( level );
			M_transport->rdbuf( M_gz_buf );
		}
		else
		{
			M_transport->rdbuf( M_socket_buf );
		}
		return M_comp_level = level;
#endif
		return M_comp_level = -1;
	}

	void processMsg(char * msg, const size_t len) { // size_t; cstdlib.h�ṩ������֮һ��size_t, wchar_t, div_t, ldiv_t, lldiv_t
#ifdef HAVE_LIBZ
		if ( M_comp_level >= 0 )
		{
			M_decomp.decompress( msg, len, Z_SYNC_FLUSH );
			char * out;
			int size;
			M_decomp.getOutput( out, size );
			if ( size > 0 )
			{
				parseMsg( out, size );
			}
		}
		else
#endif
		{
			parseMsg(msg, len); // ���� ��Ϣ����
		}
	}

	void parseMsg(char * msg, const size_t len) {
		std::cout << std::string( msg, len - 1 ) << std::endl;
		if (!std::strncmp(msg, "(ok compression", 15)) { //strncmp �Ƚ��ַ��� ��ͬ����0
			int level;
			if (std::sscanf(msg, " ( ok compression %d )", &level) == 1) { // level�����compression ���������
				setCompression(level);
			}
		} else if (!std::strncmp(msg, "(sense_body", 11) //���Ƚϣ��Ƚ�msg��sense_bodyǰ11���ַ�  ����server ���������֪��Ϣ
				|| !std::strncmp(msg, "(see_global", 11) || !std::strncmp(msg,
				"(init", 5)) { // ������������Ӿ���Ϣ���أ�eye on��������ÿ�����ڵĿ�ʼ�����������յ�ȫ���Ӿ���Ϣ
			M_clean_cycle = true;
		}


        if (!strncmp(msg, "(hear", 5)) // hear
        {
            hear(msg);
        }
        else if (!strncmp(msg, "(sense_body", 11)) // sense_body
        {
            sense(msg);
        }
        else if (!strncmp(msg, "(see", 4)) // see
        {
            see(msg);
            updateINFO(); // ������Ϣ
        }
        else
            return;


		if (iPlayerId == 1) {
			//����Ա
			if (std::strncmp(msg, "(see ", 5)) {
				return;
			}
			double ball_dist = 0;
			double ball_dir = 0;
			char command[20];
			char *pball;
			pball = strstr(msg, "(ball)"); // strstr�ڴ��в���ָ���ַ����ĵ�һ�γ���
			if (pball == 0) {
				sprintf(command, "(turn 60)"); //�ѽ�������ָ�����ַ���
				if (!sendCmd(command))
					return;
				return;
			}
			//������
			if (std::sscanf(pball, "(ball) %lf %lf", &ball_dist, &ball_dir)
					!= 2) {
				printf("get ball error\n");
				return;
			}

			//			printf("%s\t%lf\t%lf\t%lf\t%lf\n", msg, goal_dist, goal_dir,
			//					ball_dist, ball_dir);
			//����
			if (ball_dist < 3) {
				sprintf(command, "(catch %lf)", ball_dir);
				if (!sendCmd(command))
					return;
				return;
			}
			if (ball_dir > 10 || ball_dir < -10) {
				sprintf(command, "(turn %lf)", ball_dir);
				if (!sendCmd(command))
					return;
				return;
			}
		} else {
			if (!std::strncmp(msg, "(see ", 5)) {
				//see
				//get cycle
				if (std::sscanf(msg, "(see  %d )", &currentCycle) == 1) {
					if (currentCycle == 0) {
						//before_kick_off
						return;
					}
					if (currentCycle == lastCycle) {
						//before_kick_off
						return;
					}
					if (currentCycle > lastCycle) {
						kickWait++;
						if (kickWait < 3)
							return;
						double goal_dist = 0;
						double goal_dir = 0;
						double ball_dist = 0;
						double ball_dir = 0;
						char command[20];
						int len;
						int canSeeGoal;
						//kick_off
						lastCycle = currentCycle;
						//search goal
						char *pgoal;
						pgoal = strstr(msg, "(goal r)");
						if (pgoal != 0) {
							if (std::sscanf(pgoal, "(goal r) %lf %lf",
									&goal_dist, &goal_dir) != 2) {
								printf("get goal error\n");
							}
							canSeeGoal = 1;
						} else {
							canSeeGoal = 0;
						}
						char *pball;
						if (!turnToSeeGoal) {
							pball = strstr(msg, "(ball)");
							if (pball != 0) {
								if (std::sscanf(pball, "(ball) %lf %lf",
										&ball_dist, &ball_dir) != 2) {
									printf("get ball error\n");
								}
							} else {
								sprintf(command, "(turn 50)");
								if (!sendCmd(command))
									return;
								printf("turn to see ball\n");
								return;
							}
							printf("%s\t%lf\t%lf\t%lf\t%lf\n", msg, goal_dist,
									goal_dir, ball_dist, ball_dir);
							//ball turn
							if (ball_dir > 2 || ball_dir < -2) {
								sprintf(command, "(turn %lf)", ball_dir);
								if (!sendCmd(command))
									return;
								return;
							}
							//ball dash
							if (ball_dist > 0.5) {
								sprintf(command, "(dash 100)");
								if (!sendCmd(command))
									return;
								return;
							}
						}
						//kick
						turnToSeeGoal = 1;
						if (!canSeeGoal) {
							sprintf(command, "(turn 50)");
							len = strlen(command) + 1;
							printf("command:%s\n", command);
							M_transport->write(command, len);
							M_transport->flush();
							if (!M_transport->good()) {
								printf("error send socket\n");
								return;
							}
							printf("turn to see goal\n");
							return;
						}
						sprintf(command, "(kick 100 %lf)", goal_dir);
						if (!sendCmd(command))
							return;
						kickWait = 0;
						turnToSeeGoal = 0;
					}
				}
			}
		}
	}

	void messageLoop() {
		fd_set read_fds;
		fd_set read_fds_back;
		char buf[8192];
		std::memset(&buf, 0, sizeof(char) * 8192);

		int in = fileno( stdin );
		FD_ZERO(&read_fds);
		FD_SET(in, &read_fds);
		FD_SET(M_socket.getFD(), &read_fds);
		read_fds_back = read_fds;

#ifdef RCSS_WIN
		int max_fd = 0;
#else
		int max_fd = M_socket.getFD() + 1;
#endif
		while (1) {

			read_fds = read_fds_back;
			int ret = ::select(max_fd, &read_fds, NULL, NULL, NULL); //select
			if (ret < 0) {
				perror("Error selecting input");
				break;
			} else if (ret != 0) {
				// read from stdin
				if (FD_ISSET(in, &read_fds)) {
					if (std::fgets(buf, sizeof(buf), stdin) != NULL) {
						size_t len = std::strlen(buf);
						if (buf[len - 1] == '\n') {
							buf[len - 1] = '\0';
							--len;
						}

						M_transport->write(buf, len + 1);
						M_transport->flush();
						if (!M_transport->good()) {
							if (errno != ECONNREFUSED) {
								std::cerr << __FILE__ << ": " << __LINE__
										<< ": Error sending to socket: "
										<< strerror(errno) << std::endl
										<< "msg = [" << buf << "]\n";
							}
							M_socket.close();
						}
						std::cout << buf << std::endl;
					}
				}

				// read from socket
				if (FD_ISSET(M_socket.getFD(), &read_fds)) {
					rcss::net::Addr from;
					int len = M_socket.recv(buf, sizeof(buf) - 1, from);
					if (len == -1 && errno != EWOULDBLOCK) {
						if (errno != ECONNREFUSED) {
							std::cerr << __FILE__ << ": " << __LINE__
									<< ": Error receiving from socket: "
									<< strerror(errno) << std::endl;
						}
						M_socket.close();
					} else if (len > 0) {
						M_dest.setPort(from.getPort());
						M_socket_buf->setEndPoint(M_dest);
						processMsg(buf, len);
					}
				}
			}
		}
	}
};

void Client::init_pos()
{
    char command[20];
    if(iPlayerId==1)
    {
        sprintf(command,"(move %.1lf %.1lf)",initpos[iPlayerId-1].x,initpos[iPlayerId-1].y);
    }
    else{
        sprintf(command,"(move %.1lf %.1lf)",initpos[iPlayerId-1].x,initpos[iPlayerId-1].y);
    }
    myself.curCoord.x=initpos[iPlayerId-1].x;
    myself.curCoord.y=initpos[iPlayerId-1].y;
    myself.abs_dir=0;
    if (!sendCmd(command))
        return;d
    return;
}

void Client::hear(char *msg)
{
    char *hearmsg;

    if(strstr(msg,"referee"))
    {
        havingCatch=false;
        if(hearmsg=strstr(msg,"play_on"))
        {

        }
        else if(hearmsg=strstr(msg,"kick_off_"))
        {

        }
        else if(hearmsg=strstr(msg,"drop_ball"))
        {

        }
        else if(hearmsg=strstr(msg,"before_kick_off"))
        {
            init_pos();
        }
        else if(hearmsg=strstr(msg,"goal_kick"))
        {

            if(iPlayerId==1)
            {

            }
            else
            {

            }
        }
        else if(hearmsg=strstr(msg,"goal_"))
        {

        }
        else if (strstr(msg, "goalie_catch_ball_"))
        {

            if(iPlayerId==1)
            {

            }
            else
            {

            }
        }
        else if(hearmsg=strstr(msg,"corner_kick"))
        {

        }
        else if(hearmsg=strstr(msg,"free_kick"))
        {

        }
        else if(hearmsg=strstr(msg,"time_over"))
        {

        }
        else if(hearmsg=strstr(msg,"time_over"))
        {

        }
        else if(hearmsg=strstr(msg,"half_time"))
        {

        }
        else if(hearmsg=strstr(msg,"time_extended"))
        {

        }
        else if(hearmsg=strstr(msg,"offside_"))
        {

        }
        else if(hearmsg=strstr(msg,"foul_"))
        {

        }
        else
        {
            return;
        }
    }
    else
    {
        if(hearmsg=strstr(msg,"IAmPassingBall"))
        {

        }
        if(hearmsg=strstr(msg,"SuperStar"))
        {

        }
        if(strstr(msg,"SuperStardie"))
        {


        }
    }
    return;
}

namespace {
Client * client = static_cast<Client *> (0);

void sig_exit_handle(int) {
	std::cerr << "\nKilled. Exiting..." << std::endl;
	if (client) {
		delete client;
		client = static_cast<Client *> (0);
	}
	std::exit(EXIT_FAILURE);
}
}

int main(int argc, char **argv) {
	if (std::signal(SIGINT, &sig_exit_handle) == SIG_ERR || std::signal(  // �鿴csignal.h �Ĺ���
			SIGTERM, &sig_exit_handle) == SIG_ERR || std::signal(SIGHUP,
			&sig_exit_handle) == SIG_ERR) {
		std::cerr << __FILE__ << ": " << __LINE__
				<< ": could not set signal handler: " << std::strerror(errno)
				<< std::endl;
		std::exit(EXIT_FAILURE);
	}

	std::cerr << "Hit Ctrl-C to exit." << std::endl;

	std::string server = "localhost";
	int port = 6000;

	for (int i = 0; i < argc; ++i) {
		if (std::strcmp(argv[i], "-server") == 0) {
			if (i + 1 < argc) {
				server = argv[i + 1];
				++i;
			}
		} else if (std::strcmp(argv[i], "-port") == 0) {
			if (i + 1 < argc) {
				port = std::atoi(argv[i + 1]);
				++i;
			}
		}
		if (std::strcmp(argv[i], "-id") == 0) {
			if (i + 1 < argc) {
				iPlayerId = std::atoi(argv[i + 1]);
				++i;
			}
		}
		if (std::strcmp(argv[i], "-sidel") == 0) {
			iSide = 1;
		}
		if (std::strcmp(argv[i], "-sider") == 0) {
			iSide = 2;
		}
	}

	client = new Client(server, port);
	client->run();

	return EXIT_SUCCESS;
}
