[페이즈1]
execve의 환경 변수로 /usr/bin 과 /bin을 주어 이 내부의 명령어들을 사용할 수 있도록 설정했습니다.
myshell을 실행한 후 ls,pwd,echo,touch,cat,ps 등을 사용할 수 있습니다.
따옴표 설정을 수행했고, 페이즈1의 빌트인 명령어로는 cd를 두어 cd 명령어를 사용할 수 있도록 했습니다.
SIGINT와 SIGTSTP의 시그널을 무시하여 해당 CTRL+Z, CTRL+C를 사용할 수 없습니다.