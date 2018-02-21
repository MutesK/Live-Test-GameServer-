<?php

//--------------------------------------------------------------
// PHP에서 소켓을 열어 URL 호출 후 끝낸다.
//
// 본래 PHP에서는 CURL 라이브러리를 사용하여 외부 URL 호출을 하지만
// 이는 비동기 호출이 되지 않음. URL 호출 결과가 올 때까지 블럭이 걸림.
//
// 그래서 직접 소켓을 열고 웹서버에 데이터 전송 후 바로 종료.
// 물론 상대 서버로 데이터를 전송하기까지는 블럭이 걸림 하지만 웹서버의 처리 시간까지 대기하지 않음.
// 
// 결과가 필요없는 경우에만 사용.
//--------------------------------------------------------------
function http_request_async($url, $params, $type = 'POST')
{
    // 입력된 Params를 Key와 value로 분리하여, 
    // post_param 이라는 배열로 key = value 타입으로 생성
    // 혹시나 value가 배열인 경우는, 로 나열
    foreach($params as $key => &$val)
    {
        if(is_array($val))
         $val = implode(',', $val);
        
         $post_params[] = $key. '='. urlencode($val);
    }

    // $post_params 에는 [0] id = test1 / [1] pass = test1 형태로 들어감
    
    // 이를 & 기준으로 하나의 스트링으로 붙임. 주소
    $post_string = implode('&', $post_params);

    // 입력된 url을 URL요소별로 분석한다
    $parts = parse_url($url);

    if($parts['scheme'] == 'http')
        $fp = fsockopen($parts['host'], 
        isset($part['port']) ? $part['port'] : 80, 
        $errno, $errstr, 10);

    if(!$fp)
    {
        echo "Error : $errstr($errno) <br />\n";
        return 0;
    }

    $ContentLength = strlen($post_string);

    if($type == 'GET')
    {
        $parts['path'] .= '?'.$post_string;
        $ContentLength = 0;
    }

    // HTTP 프로토콜 생성
    $out = "$type ".$parts['path']." HTTP/1.1\r\n";
    $out.= "Host: ".$parts['host']." \r\n";
    $out.= "Content-Type: application/json; charset=utf-8\r\n";
    $out.= "Content-Type: application/x-www-form-urlencoded\r\n";
    $out.= "Content-Length: ".$ContentLength."\r\n";
    $out.= "Connection: Close\r\n\r\n";
    

    // Post 방식이면 프로토콜 뒤에 파라메터를 붙임
    if('POST' == $type && isset($post_string))
        $out .= $post_string;
        
    // 전송
    $Result = fwrite($fp, $out);

    fclose($fp);

    return $Result;
}



?>