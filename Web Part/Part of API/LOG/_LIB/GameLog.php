<?php

class GameLog
{
     private $LOG_URL = '';
     private $LogArray = array();
     
     public static function getInstance($GameLogURL)
     {
        static $instance; // 객체 개념
        
        if(!isset($instance))
        {
             $instance = new GameLog();
        }

        $instance->LOG_URL = $GameLogURL; 
    
        return $instance;
     }
    

     function AddLog($accountNo, $type, $code, $param1 = 0, $param2 = 0, $param3 = 0, $param4 = 0, $paramString = "NONE")
     {
        //로그를 모아서 한방에 쏠거기 때문에, 멤버로 만들어둔 LogArray 변수에 로그를 쌓아둠
         array_push($this->LogArray, array("AccountNo" => $accountNo,
         "Type" => $type,
         "Code" => $code,
         "Param1" => $param1,
         "Param2" => $param2,
         "Param3" => $param3,
         "Param4" => $param4,
         "ParamString" => $paramString,
         ));
     }
     
     function SaveLog()
     {
        //배열에 저장된 로그를 비동기 http 요청

        foreach($this->LogArray as $var)
        {
          $postField = array("LogChunk" => json_encode($var));

          http_request_async($this->LOG_URL, $postField, "POST");
        }
     }
  }
  ?>