#include "LineCluster.h"


void FSegmentCluster::GenerateLODLevel()
{
	// 保存原始LOD0的线段
	TArray<FSegment> OriginalSegments = Segments;
	int32 OriginalNum = OriginalSegments.Num();

	// 初始化LOD0的线段数量
	SegmentNumPerLOD[0] = OriginalNum;

	// 如果原始线段数量不足，直接复制原始线段到各个LOD级别
	if (OriginalNum <= 1)
	{
		for (int32 LOD = 1; LOD < 8; ++LOD)
		{
			SegmentNumPerLOD[LOD] = SegmentNumPerLOD[0];
			if (OriginalNum > 0)
			{
				Segments.Append(OriginalSegments);
			}
		}
		return;
	}

	// 为每个LOD层级生成线段
	TArray<FSegment> CurrentLODSegments = OriginalSegments;
	for (int32 LODLevel = 1; LODLevel < 8; ++LODLevel)
	{
		TArray<FSegment> SimplifiedSegments;

		int32 CurrentSegmentNum = CurrentLODSegments.Num();
		// 如果当前线段数量已经很少，直接复制
		if (CurrentSegmentNum <= 1)
		{
			SimplifiedSegments = CurrentLODSegments;
			SegmentNumPerLOD[LODLevel] = SimplifiedSegments.Num();
			Segments.Append(SimplifiedSegments);
			CurrentLODSegments = SimplifiedSegments;
			continue;
		}


		// 夹角阈值（15度，约0.26弧度）
		const float AngleThreshold = 15.0f * PI / 180.0f;

		// 期望目标线段数量为上一层级的一半
		int32 TargetSegmentNum = FMath::Max(1, CurrentSegmentNum >> 1);
		int32 i = 0;
		while (i < CurrentSegmentNum)
		{
			// 获取当前线段
			FSegment CurrentSegment = CurrentLODSegments[i];

			// 如果这是最后一个线段，直接添加
			if (i == CurrentSegmentNum - 1)
			{
				SimplifiedSegments.Add(CurrentSegment);
				break;
			}

			// 检查是否可以与后续线段合并
			bool bMerged = false;
			int32 j = i + 1;

			while (j < CurrentSegmentNum)
			{
				FSegment NextSegment = CurrentLODSegments[j];

				// 计算当前线段和下一个线段的方向向量
				FVector CurrentDir = (CurrentSegment.End - CurrentSegment.Start).GetSafeNormal();
				FVector NextDir = (NextSegment.End - NextSegment.Start).GetSafeNormal();

				// 检查线段是否连续（端点匹配）
				bool bIsContinuous = FVector::Distance(CurrentSegment.End, NextSegment.Start) < 0.1f;

				if (bIsContinuous)
				{
					// 计算夹角（使用点积）
					float DotProduct = FVector::DotProduct(CurrentDir, NextDir);
					float Angle = FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f));

					// 如果夹角小于阈值，合并线段
					if (Angle < AngleThreshold * LODLevel)
					{
						// 创建合并后的线段：使用第一个线段的起点和最后一个合并线段的终点
						FSegment MergedSegment(CurrentSegment.Start, NextSegment.End, CurrentSegment.PolygonIndex);

						CurrentSegment = MergedSegment;
						j++;
						TargetSegmentNum--;
						bMerged = true;
					}
					else
					{
						// 夹角太大，停止合并
						break;
					}
				}
			}

			// 添加合并后的线段或原始线段
			SimplifiedSegments.Add(CurrentSegment);

			// 更新索引
			if (bMerged)
			{
				i = j; // 跳过所有已合并的线段
			}
			else
			{
				i++; // 移动到下一个线段
			}
		}

		// 记录当前LOD级别的线段数量
		SegmentNumPerLOD[LODLevel] = SimplifiedSegments.Num();

		// 将简化后的线段添加到总数组
		Segments.Append(SimplifiedSegments);

		// 更新为当前简化结果，用于下一级LOD生成
		CurrentLODSegments = SimplifiedSegments;
	}
}