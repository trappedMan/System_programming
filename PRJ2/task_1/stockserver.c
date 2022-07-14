#include "csapp.h"

typedef struct _item{
	int ID;	//stock id
	int left_stock;
	int price;
	int readcnt;	//using for Readers-Writers problem
	sem_t mutex;	//binary semaphore for locking node
	struct _item* left;
	struct _item* right;
}item;	//struct for tree node
typedef struct{
	int maxfd;
	fd_set read_set;	//searching target
	fd_set ready_set;	//triggered fds
	int nready;
	int maxi;
	int clientfd[FD_SETSIZE];	//store connfd
	rio_t clientrio[FD_SETSIZE];
}_pool;	//struct for io multiplexing

item *root;	//tree root
int printflag,noprintflag;//by this two flag ,updating "stock.txt"  
//struct timeval start,end;
//unsigned long e_usec;

void echo(int connfd);
void insert_node(item* elem){	//insert node in tree(actually called N_stock times when server process executes initially)
	if(!root){
		root=elem;
		return;
	}
	item *ptr=root;
	while(1){
		if(ptr->ID>elem->ID){
			if(ptr->left)
				ptr=ptr->left;
			else{
				ptr->left=elem;
				break;
			}
		}
		else{
			if(ptr->right)
				ptr=ptr->right;
			else{
				ptr->right=elem;
				break;
			}
		}
	}
}
void show_tree(char* buf,item* rootitem){	//traversing tree and store the information in buf
	char buff[50];
	if(!rootitem){
		return;
	}
	show_tree(buf,rootitem->left);
	sprintf(buff,"%d %d %d\n",rootitem->ID,rootitem->left_stock,rootitem->price);	
	strcat(buf,buff);
	show_tree(buf,rootitem->right);
}

item* search_tree(item* rootitem,int key){	//search specific item
	if(rootitem->ID>key)
		return search_tree(rootitem->left,key);
	else if(rootitem->ID<key)
		return search_tree(rootitem->right,key);
	else
		return rootitem;
}
void free_tree(item* rootitem){	//deallocate whole tree.
	if(rootitem->left){
		free_tree(rootitem->left);
	}
	if(rootitem->right){
		free_tree(rootitem->right);
	}
	free(rootitem);
}

void buy(char *buf){	//buy stock(update the tree)
	char tmpstr[5];
	item* tmpitem;
	int tmpid, tmpnum;

	sscanf(buf,"%s %d %d",tmpstr,&tmpid,&tmpnum);
	tmpitem=search_tree(root,tmpid);
	if(tmpitem->left_stock>=tmpnum){
		strcpy(buf,"[buy] success\n");
		(tmpitem->left_stock)-=tmpnum;
	}
	else{
		strcpy(buf,"Not enough left stock\n");
	}
}

void sell(char* buf){	//sell stock(update the tree)
	char tmpstr[5];
	item* tmpitem;
	int tmpid, tmpnum;

	sscanf(buf,"%s %d %d",tmpstr,&tmpid,&tmpnum);
	tmpitem=search_tree(root,tmpid);
	strcpy(buf,"[sell] success\n");
	(tmpitem->left_stock)+=tmpnum;
}
void init_pool(int listenfd, _pool *p){	//initialize pool struct.
	int i;
	p->maxi=-1;
	for(i=0;i<FD_SETSIZE;i++){
		p->clientfd[i]=-1;
	}
	p->maxfd=listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd,&p->read_set);
}

void add_client(int connfd,_pool *p){	//changing pool with accepted client information
	int i;
	p->nready--;
	for(i=0;i<FD_SETSIZE;i++){
		if(p->clientfd[i]<0){
			p->clientfd[i]=connfd;
			Rio_readinitb(&p->clientrio[i],connfd);

			FD_SET(connfd,&p->read_set);

			if(connfd>p->maxfd)
				p->maxfd=connfd;
			if(i>p->maxi)
				p->maxi=i;
			break;
		}
	}
	if(i==FD_SETSIZE)
		perror("fd set is full\n");
}

void check_clients(_pool *p){	//traversing ready set, and process it.
	int i,connfd,n;
	char buf[MAXLINE];
	rio_t rio;
	
	for(i=0;(i<=p->maxi)&&(p->nready>0);i++){
		connfd=p->clientfd[i];
		rio=p->clientrio[i];

		if((connfd>0)&&(FD_ISSET(connfd,&p->ready_set))){
			p->nready--;
			if((n=Rio_readlineb(&rio,buf,MAXLINE))!=0){
				if(!strncmp(buf,"show",4)){
					printf("server received %d bytes\n",n);
					strcpy(buf,"");
					show_tree(buf,root);
					Rio_writen(connfd,buf,MAXLINE);
				}
				else if(!strncmp(buf,"buy",3)){
					printf("server received %d bytes\n",n);
					buy(buf);
					Rio_writen(connfd,buf,MAXLINE);
				}
				else if(!strncmp(buf,"sell",4)){	
					printf("server received %d bytes\n",n);
					sell(buf);
					Rio_writen(connfd,buf,MAXLINE);
				}
				else if(!strncmp(buf,"exit",4)){	
					printf("server received %d bytes\n",n);
					Close(connfd);
					FD_CLR(connfd,&p->read_set);
					printf("fd %d disconnected\n",connfd);
					p->clientfd[i]=-1;
				}
			}
			else{
				Close(connfd);
				FD_CLR(connfd,&p->read_set);
				printf("fd %d disconnected\n",connfd);
				p->clientfd[i]=-1;
			}
		}
	}
	FILE* printfd;
	printflag=1;
	for(i=0;i<=p->maxi;i++){
		if(p->clientfd[i]>0){	//if even a client now connected with server, no print
			printflag=0;
			break;
		}
	}
	if(printflag&&!noprintflag){	//if this condition true, store the whole tree to "stock.txt"
		//gettimeofday(&end,0);
		//printf("time counting ends\n");
		//e_usec=(end.tv_sec*1000000+end.tv_usec)-(start.tv_sec*1000000+start.tv_usec);
		//printf("elapsed time for event driven approach : %lu usec\n",e_usec);
		noprintflag=1;
		printfd=fopen("stock.txt","w");
		strcpy(buf,"");
		show_tree(buf,root);
		fprintf(printfd,"%s",buf);
		fclose(printfd);
	}
}
void sigint_handler(int sig){
	free_tree(root);
	exit(0);
}
int main(int argc, char **argv) 
{
	Signal(SIGINT,sigint_handler);	//server always terminates with SIGINT, so sigint handler deallocates tree nodes
    int listenfd, connfd;
	int tmpid,tmpst,tmpprice;
	FILE* stockf;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
	static _pool pool;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }	
	noprintflag=1;
	stockf=fopen("stock.txt","r");
	while(fscanf(stockf,"%d %d %d",&tmpid,&tmpst,&tmpprice)!=EOF){	//constructing stock binary tree
		item *tmpitem=(item*)malloc(sizeof(item));
		tmpitem->ID=tmpid;
		tmpitem->left_stock=tmpst;
		tmpitem->price=tmpprice;		
		tmpitem->readcnt=0;
		Sem_init(&(tmpitem->mutex),0,1);
		tmpitem->left=tmpitem->right=NULL;
		insert_node(tmpitem);
	}
	fclose(stockf);
	
	
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd,&pool);
	while (1) {
		pool.ready_set=pool.read_set;
		pool.nready=Select(pool.maxfd+1,&pool.ready_set,NULL,NULL,NULL);	//read set->ready set 

		if(FD_ISSET(listenfd,&pool.ready_set)){	//connect new client
			clientlen=sizeof(struct sockaddr_storage);
			connfd=Accept(listenfd,(SA*)&clientaddr,&clientlen);
			Getnameinfo((SA*)&clientaddr,clientlen,client_hostname,MAXLINE,client_port,MAXLINE,0);
			printf("Connected to (%s %s)\n",client_hostname,client_port);
			if(noprintflag){
				noprintflag=0;
				//printf("time counting starts\n");
				//gettimeofday(&start,0);
			}
			add_client(connfd,&pool);

		}
		check_clients(&pool);	//processing ready set
    }
    exit(0);
}
