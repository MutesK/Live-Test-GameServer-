<?php

/*
    PHP 에러를 핸들링하여 시스템 로그로 남기는 코드

    * 필수 사항

    lib_LOG.php 가 include 되어 있어야 함
*/

////////////////////////////////////////////////
// PHP 종료 로그용
////////////////////////////////////////////////
function sys_Shutdown()
{
    // PHP 파일 종료시 처리되지 못한 에러가 있는 경우
    // 이를 받아서 에러 핸들러로 강제 호출

    $error = error_get_last();
    if(isset($error) && ($error['type'] & 1))
    {
        ERROR_Handler($error['type'], $error['message'],
        $error['file'], $error['line']);
    }
}
////////////////////////////////////////////////
// 셧다운 핸들러 : PHP 파일이 종료될때 자동으로 특정 함수를 호출
////////////////////////////////////////////////
register_shutdown_function('sys_Shutdown');

////////////////////////////////////////////////
// 중간에 연결이 끊길 시 php 처리를 중단하지 않는다.
////////////////////////////////////////////////
ignore_user_abort(true);

////////////////////////////////////////////////
// 에러 핸들러 등록
// E_ALL 모든 에러를 추력. 이는 php.ini에서 설정도 가능
// 에러 발생시 ERROR_Handler 호출
// 예외 발생시 EXCEPTION_Handler 호출
// 이 후에는 신텍스 문법에러 외에는 화면에 에러가 출력되지 않음.
////////////////////////////////////////////////
error_reporting(E_ALL);
set_error_handler('ERROR_Handler');
set_exception_handler('EXCEPTION_Handler');

////////////////////////////////////////////////
//  PHP 예외 로그 처리
////////////////////////////////////////////////
function ERROR_Handler($errno, $errstr, $errfile, $errline)
{
    global $g_AccountNo;

    // 전역 g_AccountNo 가 잇다는 가정
    // 누구로 인한 에러인지 확인하기 위해 에러 발생시 AccountNo 를 확인함.
    if(!isset($g_AccountNo))
        $g_AccountNo = -1;

    // 처리 하고자 하는 에러 레벨이 아니면 패스.
    if(!(error_reporting() & $errno))
        return;

    $ErrorMsg = "($errno) FILE: $errfile / LINE: $errline / MSG: $errstr";

    // 조금 더 상세한 확인을 위해 종류별로 메세지를 약간 변경
    // 하지만 큰 의미는 없음.

    switch($errno)
    {
        case E_USER_ERROR:
            $ErrorString = "E_USER_ERROR ". $ErrorMsg;
            break;
        case E_USER_WARNING:
            $ErrorString = "E_USER_WARNING ". $ErrorMsg;
            break;
         case E_USER_NOTICE:
            $ErrorString = "E_USER_NOTICE ". $ErrorMsg;
            break;
        default:
            $ErrorString = "etc_Error ". $ErrorMsg;
            break;
    }

    // 발생된 에러를 DB에 저장
    LOG_System($g_AccountNo, "ERROR_Handler", $ErrorString);

    // Account 로드 후 에러가 난 경우 에러 상황을 클라에게 넘긴다.
    if($g_AccountNo > 0)
    {
        // 유저 인증이 된 후 에러라면 컨텐츠 처리 부분에서의 에러라고 봐야 한다
        // 그로므로 해당 클라에게 에러발생 메세지를 전송해주는 부분이 들어가야 함.

        // ...
    }

    exit;
}

function EXCEPTION_Handler($exception)
{
    global $g_AccountNo;

    if(isset($g_AccountNo) === FALSE)
        $g_AccountNo = -1;

    LOG_System($g_AccountNo, $exception->getMessage());

    exit;
}

?>