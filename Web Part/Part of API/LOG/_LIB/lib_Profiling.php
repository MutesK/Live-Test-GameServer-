<?php
  
  // 프로파일링 로그 POST 전달 내용
// POST 방식으로 프로파일링 로그를 저장한다.
//
// $_POST['IP']			: 유저
// $_POST['MemberNo']	: 유저   or AccountNo  편의에 따라서 ~
// $_POST['Action']		: 액션구분
//
// $_POST['T_Page']			: Time Page
// $_POST['T_Mysql_Conn']	: Time Mysql Connect
// $_POST['T_Mysql']		: Time Mysql
// $_POST['T_ExtAPI']		: Time API
// $_POST['T_Log']			: Time Log
// $_POST['Query']			: Mysql 이 있는 경우 쿼리문
// $_POST['Comment']		: 그 외 기타 멘트

  class Profiling
  {
     private $LOG_FLAG = false;
     private $LOG_URL = '';
     private $PROFILING = array();
   
     private $ACTION = '';
     private $QUERY = '';
     private $COMMENT = '';
   
     function __construct()
     {
         $this->_PROFILING['T_Page']['start'] = 0;
         $this->_PROFILING['T_Page']['sum'] = 0;

         $this->_PROFILING['T_Mysql_Conn']['start'] = 0;
         $this->_PROFILING['T_Mysql_Conn']['sum'] = 0;

         $this->_PROFILING['T_Mysql']['start'] = 0;
         $this->_PROFILING['T_Mysql']['sum'] = 0;

         $this->_PROFILING['T_ExtAPI']['start'] = 0;
         $this->_PROFILING['T_ExtAPI']['sum'] = 0;

         $this->_PROFILING['T_Log']['start'] = 0;
         $this->_PROFILING['T_Log']['sum'] = 0;
     }
   
     public static function getInstance($LOG_URL, $COMMENT, $logRate)
     {      
        static $instance;

        if(!isset($instance))
            $instance = new Profiling();

        srand(time());
        $Rand = rand(0, 10000);

        if($Rand <= $logRate)
            $instance->LOG_FLAG = true;
        else
            $instance->LOG_FLAG = false;

         $instance->LOG_URL = $LOG_URL;
         $instance->COMMENT = $COMMENT;

        return $instance;
     }

     function startCheck($type)
     {
       // 로그플래그 확인
       if($this->LOG_FLAG)
         $this->_PROFILING[$type]['start'] = microtime(TRUE);
     }
   
     function stopCheck($type, $comment = '')
     {
        //로그플래그 확인
        if($this->LOG_FLAG)
             $this->_PROFILING[$type]['sum'] += microtime(TRUE) - $this->_PROFILING[$type]['start'];
        
     }
   
     function saveLog()
     {
        //로그플래그 확인
        if($this->LOG_FLAG)
        {
            global $g_AccountNo;

            if(!isset($g_AccountNo) || $g_AccountNo == 0)
                $g_AccountNo = -1;

            echo 'AccountNo :' . $g_AccountNo;
    
            $postField = array(
                    "IP" => array_key_exists('HTTP_X_FORWARDED_FOR', $_SERVER) ? $_SERVER['HTTP_X_FORWARDED_FOR'] : $_SERVER['REMOTE_ADDR'],
                    "MemberNo" => $g_AccountNo,
                    "Action" => $this->ACTION,
                    "T_Page" => $this->_PROFILING['T_Page']['sum'],
                    "T_Mysql_Conn" => $this->_PROFILING['T_Mysql_Conn']['sum'],
                    "T_Mysql" => $this->_PROFILING['T_Mysql']['sum'],
                    "T_ExtAPI" => $this->_PROFILING['T_ExtAPI']['sum'],
                    "T_Log" => $this->_PROFILING['T_Log']['sum'],
                    "Query" => $this->QUERY,
                    "Comment" => $this->COMMENT
            );

            http_request_async($this->LOG_URL, $postField, "POST");
        }
        
     }
  }

?>