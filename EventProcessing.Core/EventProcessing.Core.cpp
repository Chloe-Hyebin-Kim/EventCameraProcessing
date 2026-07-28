// EventProcessing.Core.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "pch.h"
#include "framework.h"

// TODO: 라이브러리 함수의 예제입니다.
void fnEventProcessingCore()
{
	//1. event stream
	//2. accumulation image
	//3. noise filtering
	//4. ball candidate detection


	///////////
	//	1. Metavision EVK4 HD RAW 입력 구조
	//	2. event accumulation image 생성
	//	3. frame image 입력 구조
	//	4. event - frame timestamp 정렬
	//	5. ball ROI 검출
	//	6. ROI 내부 dimple candidate 검출
	//	7. 연속 구간 dimple flow 추정
	//	8. spin axis 추정 실험 코드
	////////////////////////////////////////////
	//	[1] Event.h
	//	timestamp, x, y, polarity
	//
	//	[2] MetavisionRawLoader 또는 //MetavisionEventSource
	//	EVK4 HD RAW 입력
	//
	//	[3] EventAccumulator
	//	특정 시간 window의 event image 생성
	//
	//	[4] FrameImageLoader
	//	동축 frame camera image 입력
	//
	//	[5] TimestampAligner
	//	event time window와 frame timestamp 매칭
	//
	//	[6] BallDetector
	//	공 ROI / center / radius 검출
	//
	//	[7] DimpleCandidateDetector
	//	ROI 내부 딤플 후보점 검출
	//
	//	[8] EventFrameFusion
	//	frame image + event accumulation 결합
	//
	//	[9] Debug output
	//	positive event image
	//	negative event image
	//	merged event image
	//	frame image
	//	fused image
	//	ball ROI result
}	//
