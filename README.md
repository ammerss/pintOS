프로젝트 요구 사항 
===================
(1) 가상 메모리 정책 구현 및 성능 평가 
------------------------------------
###	요구사항 1: Page Allocation 정책 구현  
Pintos 프로젝트 디렉터리 내 ‘threads/palloc.c’등의 소스코드를 분석하고, 현재 First Fit으로 구현된 연속된 페이지 할당 정책에 Next Fit, Best Fit, 그리고 Buddy System 을 추가한다. 이는 User Pool과 Kernel Pool에 동시에 적용될 수 있어야 한다. 

###	요구사항 2: Page Allocation 정책의 성능 평가
요구사항 1에서 구현한 연속 페이지 할당 정책들을 비교할 수 있도록 페이지 할당의 성능을 테스트 하는 프로그램을 작성한다. 여러분은 페이지를 할당하면서 Page Pool의 Fragmentation 상태나 평균 탐색량 등 성능 평가를 위한 측정 기준을 정의하고 테스트 프로그램에 반영해야 한다. 

###	요구사항 3: Frame Table 구현 
Pintos 프로젝트 디렉터리 내에 있는 ‘userprog’, ‘threads/palloc.c’ 등의 소스를 분석하고, User Program을 실행할 때 할당하는 User Page들을 관리하는 Frame Table을 구현한다. Frame Table에서 각 엔트리는 시스템의 Main Memory에서 User Process에 할당된 페이지를 가리키는 포인터가 저장되어 있다. Frame Table은 더이상 User 프로세스에 할당 가능한 페이지가 없을 경우, Frame Table에서 스왑 대상이 될 페이지를 찾아 그 데이터를 스왑 장치로 스왑 아웃하기 위해 사용된다. User Page를 위한 Page Frame은 User Pool에서 가져와야 하며, 이는 함수‘palloc_get_page(PAL_USER)’ 호출을 통해 이루어진다. 

###	요구사항 4: Page Fault Handler 구현 
Pintos 프로젝트 디렉터리 내에 있는 userprog 소스를 분석하고, Page Fault 발생 시 다음의 작업을 수행하는 Page Fault Handler를 구현한다. 기본적인 Page Fault Handler는 ‘userprog/exception.c’에 구현되어 있다. 
*	메모리 주소가 Valid 하면 User Program 실행 파일에서 페이지를 가져와야 하는지, 스왑 슬롯에서 페이지를 가져와야 하는지 판단한다. 또는 빈(Zero Page Contents, 0으로 채워진) 페이지를 요청할 수도 있다. User Program 실행 파일에서 페이지를 가져오기 위해 필요한 정보들은 ‘userprog/process.c’의 ‘load ( )’ 함수를 참고해 얻을 수 있다. 
*	Kernel Virtual Address 요청, read-only page에 write, 또는 User Process가 요청하는 주소에 아무 데이터도 없는 경우, 이것은 Invalid한 요청이므로 프로세스를 종료하고 모든 리소스를 해제(release)한다. 
*	(요구사항 3)에서 구현한 Frame Table을 통해 사용중이지 않은 페이지를 얻는다. 
*	파일시스템이나 스왑 슬롯으로부터 데이터를 읽어 페이지에 데이터를 저장하거나 0으로 페이지를 초기화한다. 
*	Frame Table을 통해 얻은 사용중이지 않은 페이지의 Physical Address를 Page Fault가 발생한 Virtual Address에 매핑한다. 이 기능은 ‘userprog/pagedir.c’의 ‘pagedir_set_page ( )’ 함수를 통해 수행할 수 있다. 

###	요구사항 5: 스왑 기능 구현 
(요구사항 3)에서 할당 가능한 미사용 페이지가 없을 경우, 기존 할당된 페이지를 스왑 장치로 스왑 아웃할 수 있도록 스왑 기능을 구현한다. Pintos에서는 스왑 장치에 페이지 데이터를 저장할 수 있는 기능을 제공한다. 여러분은 ‘device/block.c’ 를 참고해 스왑 장치에 읽기/쓰기 작업을 수행할 수 있다. 예를 들어, block_get_role() 함수를 호출하면서 매개변수로서 BLOCK_SWAP (enum 정의된 변수)를 전달하면, 함수는 스왑 장치에 대한 Block Device 구조체 포인터를 반환한다. 여러분은 이 포인터를 이용해 스왑 장치에 block_read( ) 또는 block_write( ) 연산을 수행할 수 있다.
*	페이지를 스왑 장치로 스왑 아웃 하는 경우, 빈 스왑 슬롯을 찾아 페이지의 데이터를 저장한다. 
*	빈 스왑 슬롯이 없는 경우, Replacement Policy를 적용한다. 여러분은 Least Recently Used(LRU), Clock, Second Chance 정책을 각각 구현하고, 성능 측정을 한다. 
*	페이지를 스왑 장치로부터 읽어온 경우, 또는 어떤 프로세스의 페이지가 스왑 장치에 저장되어 있는 도중 프로세스가 종료된 경우, 해당 스왑 슬롯을 해제한다. 

###	요구사항 6: 스왑 Replacement Policy의 성능 평가  
요구사항 5에서 구현한 스왑 Replacement Policy의 성능 비교를 위한 성능 측정 프로그램을 구현한다. 각 Replacement Policy를 비교하기 위해 어떤 성능 측정 척도가 필요한지 제안하고, 성능 측정 프로그램을 examples 디렉터리에 구현한다. 

###	요구사항 7: 보고서 작성  
요구사항 1~6까지 구현하고 수행한 모든 작업을 보고서로 작성한다. 보고서에는 각 요구사항 별 문제 해결 방법, 요구사항 수행(개발 또는 성능측정) 내용과 결과 등을 상세하게 기술해야 한다. 성능 평가 척도의 경우, 예를 들어 Replacement Policy에서 성능 측정 척도를 Hit/Miss Ratio로 선정하였을 경우 그렇게 선정한 이유를 기술한다. 

