#define NTHREAD 100	//number of using thread. if this value 100, only 100 threads can run.
#include "csapp.h"
typedef struct _item{
	int ID;
	int left_stock;
	int price;
	int readcnt;	//number of threads now reading this item
	sem_t mutex;	//semaphore for mutual exclusion
	sem_t wr;	//if readcnt not zero, wr is zero(locked)
	struct _item* left;
	struct _item* right;
}item;
item* root;	//tree root
typedef struct {
	int *buf;	//main buffer
	int n;
	int front;
	int rear;
	sem_t mutex;	//semaphore for mutual exclusion
	sem_t slots;	//counting semaphore
	sem_t items;	//counting semaphore
}sbuf_t;	//struct for producer-consumer model
sbuf_t sbuf;
int threadcnt,noprintflag;	//using for updating "stock.txt"
sem_t threadmtx;	//using when some codes that should not be interrupted by other threads. 
//struct timeval start,end;
//unsigned long e_usec;

void echo(int connfd);

void sbuf_init(sbuf_t *sp,int n){	//initialize sbuf
	sp->buf=Calloc(n,sizeof(int));
	sp->n=n;
	sp->front=sp->rear=0;
	Sem_init(&sp->mutex,0,1);
	Sem_init(&sp->slots,0,n);
	Sem_init(&sp->items,0,0);
}
void sbuf_deinit(sbuf_t *sp){	//deallocate buf in sbuf struct
	Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp,int item){	//insert new item in sbuf
	P(&sp->slots);
	P(&sp->mutex);
	sp->buf[(++sp->rear)%(sp->n)]=item;
	V(&sp->mutex);
	V(&sp->items);
}
int sbuf_remove(sbuf_t *sp){	//remove item from sbuf, and bring it.
	int item;
	P(&sp->items);
	P(&sp->mutex);
	item=sp->buf[(++sp->front)%(sp->n)];
	V(&sp->mutex);
	V(&sp->slots);
	return item;
}
void insert_node(item* elem){	//insert tree nodes
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
void show_tree(char* buf,item* rootitem){	//traversing tree and store the information to buf.
	char buff[50];
	if(!rootitem)
		return;
	show_tree(buf,rootitem->left);

	P(&rootitem->mutex);	//modify readcnt(cannot be interrupted)
	(rootitem->readcnt)++;	
	if(rootitem->readcnt==1)
		P(&rootitem->wr);	//when reader threads starts reading, wr locked
	V(&rootitem->mutex);

	sprintf(buff,"%d %d %d\n",rootitem->ID,rootitem->left_stock,rootitem->price);
	
	P(&rootitem->mutex);
	(rootitem->readcnt)--;
	if(rootitem->readcnt==0)
		V(&rootitem->wr);	//all reader finished reading, wr unlocked
	V(&rootitem->mutex);

	strcat(buf,buff);
	show_tree(buf,rootitem->right);
}

item* search_tree(item* rootitem,int key){
	P(&rootitem->mutex);
	(rootitem->readcnt)++;
	if(rootitem->readcnt==1)
		P(&rootitem->wr);
	V(&rootitem->mutex);

	//codes under this line use node information only checking the if condition. so immediately decrement readcnt.
	if(rootitem->ID>key){
		P(&rootitem->mutex);
		(rootitem->readcnt)--;
		if(rootitem->readcnt==0)
			V(&rootitem->wr);
		V(&rootitem->mutex);
		return search_tree(rootitem->left,key);
	}
	else if(rootitem->ID<key){
		P(&rootitem->mutex);
		(rootitem->readcnt)--;
		if(rootitem->readcnt==0)
			V(&rootitem->wr);
		V(&rootitem->mutex);
		return search_tree(rootitem->right,key);
	}
	else{

		P(&rootitem->mutex);
		(rootitem->readcnt)--;
		if(rootitem->readcnt==0)
			V(&rootitem->wr);
		V(&rootitem->mutex);
		return rootitem;
	}
}

void free_tree(item* rootitem){
	if(rootitem->left)
		free_tree(rootitem->left);
	if(rootitem->right)
		free_tree(rootitem->right);
	free(rootitem);
}

void buy(char *buf){
	char tmpstr[5];
	item* tmpitem;
	int tmpid,tmpnum;

	sscanf(buf,"%s %d %d",tmpstr,&tmpid,&tmpnum);
	tmpitem=search_tree(root,tmpid);
	P(&tmpitem->wr);
	if(tmpitem->left_stock>=tmpnum){
		strcpy(buf,"[buy] success\n");
		(tmpitem->left_stock)-=tmpnum;
	}
	else
		strcpy(buf,"Not enough left stock\n");
	V(&tmpitem->wr);
}

void sell(char* buf){
	char tmpstr[5];
	item* tmpitem;
	int tmpid,tmpnum;

	sscanf(buf,"%s %d %d",tmpstr,&tmpid,&tmpnum);
	tmpitem=search_tree(root,tmpid);
	P(&tmpitem->wr);
	strcpy(buf,"[sell] success\n");
	(tmpitem->left_stock)+=tmpnum;
	V(&tmpitem->wr);
}

void *thread(void *vargp){	//thread routine. essence for this program
	int connfd,n;
	char buf[MAXLINE];
	rio_t rio;
	FILE* printfd;

	Pthread_detach(pthread_self());	//detached mode
	while(1){
		connfd=sbuf_remove(&sbuf);
		P(&threadmtx);	//lock other threads
		threadcnt++;
		V(&threadmtx);
		Rio_readinitb(&rio,connfd);
		while(1){
			if((n=Rio_readlineb(&rio,buf,MAXLINE))!=0){
				if(!strncmp(buf,"show",4)){
					printf("server received %d bytes\n",n);
					strcpy(buf,"");
					show_tree(buf,root);	//after this call, buf filled with tree information
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
					printf("fd %d disconnected by exit message\n",connfd);
					
					P(&threadmtx);
					threadcnt--;
					if(threadcnt==0&&!noprintflag){
						noprintflag=1;
						//gettimeofday(&end,0);
						//e_usec=((end.tv_sec*1000000)+end.tv_usec)-((start.tv_sec*1000000)+start.tv_usec);
						//printf("elapsed time for thread based approach : %lu usec\n",e_usec);
						printfd=fopen("stock.txt","w");
						strcpy(buf,"");
						show_tree(buf,root);
						fprintf(printfd,"%s",buf);
						fclose(printfd);
					}
					V(&threadmtx);	
					break;
					
				}
			}
			
		
			else{
				printf("fd %d disconnected\n",connfd);
				Close(connfd);
				P(&threadmtx);
				threadcnt--;
				if(threadcnt==0&&!noprintflag){
					//gettimeofday(&end,0);
					//printf("time counting ends\n");
					//e_usec=((end.tv_sec*1000000)+end.tv_usec)-((start.tv_sec*1000000)+start.tv_usec);
					//printf("elapsed time for thread based approach : %lu usec\n",e_usec);
					noprintflag=1;
					printfd=fopen("stock.txt","w");
					strcpy(buf,"");
					show_tree(buf,root);
					fprintf(printfd,"%s",buf);
					fclose(printfd);
				}
				V(&threadmtx);	
				break;
			}
		}
	}
		return NULL;
}
void sigint_handler(int sig){
	free_tree(root);
	sbuf_deinit(&sbuf);
	exit(0);
}
int main(int argc, char **argv) 
{
	Signal(SIGINT,sigint_handler);
    int i,listenfd, connfd;
	int tmpid,tmpst,tmpprice;
	FILE* stockf;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid;
	threadcnt=0;noprintflag=1;
	Sem_init(&threadmtx,0,1);

    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }
	
	stockf=fopen("stock.txt","r");
	while(fscanf(stockf,"%d %d %d",&tmpid,&tmpst,&tmpprice)!=EOF){
		item *tmpitem=(item *)malloc(sizeof(item));
		tmpitem->ID=tmpid;
		tmpitem->left_stock=tmpst;
		tmpitem->price=tmpprice;
		tmpitem->readcnt=0;
		Sem_init(&(tmpitem->mutex),0,1);
		Sem_init(&(tmpitem->wr),0,1);
		tmpitem->left=tmpitem->right=NULL;
		insert_node(tmpitem);
	}
	fclose(stockf);

    listenfd = Open_listenfd(argv[1]);
	sbuf_init(&sbuf,1020);	//FD_SETSIZE-3(reserved)-1(assigned for listenfd)=1020
    for(i=0;i<NTHREAD;i++){
		Pthread_create(&tid,NULL,thread,NULL);
	}
	while (1) {
		clientlen = sizeof(struct sockaddr_storage);
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("Accept to (%s, %s)\n", client_hostname, client_port);	
		P(&threadmtx);
		if(threadcnt==0){
			noprintflag=0;
			//printf("time counting starts\n");
			//gettimeofday(&start,0);
		}
		V(&threadmtx);
		sbuf_insert(&sbuf,connfd);
    }
    exit(0);
}
