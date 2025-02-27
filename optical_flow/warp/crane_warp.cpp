// C && C++
#include <list>
#include <cmath>
#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>


template<typename S=float, typename T=unsigned char>
inline T clip(const S x, const T lhs, const T rhs) {
	if (x < lhs) return lhs;
	else if (x > rhs) return rhs;
	else return x;
}




template<typename T=int>
inline T nearest_round(const float x, const float eps=0.5f) {
	return T(x + eps);
}



template<typename type=unsigned char, const int dimension>
void backward_warp_using_flow_inplementation(
		type* result, 
		type* source,
		float* flow,
		int height, 
		int width, 
		int channel,
		const char* mode) {
	// 是否用双线性
	bool use_bilinear = std::strcmp(mode, "bilinear") == 0;
	bool use_nearest  = std::strcmp(mode, "nearest") == 0;
	// std::cout << std::boolalpha << "use_bilinear  :  " << use_bilinear << std::endl;
	// std::cout << std::boolalpha << "use_nearest   :  " << use_nearest << std::endl;
	// 遍历每一个位置
	for (int i = 0; i < height; ++i) {
		float* flow_ptr = flow + i * width * dimension;
		for (int j = 0; j < width; ++j) {
			// 获取当前坐标 和 对应的光流值
			float x = i + flow_ptr[2 * j + 1];
			float y = j + flow_ptr[2 * j];
			// 截断
			x = clip(x, 0.f, height - 1.00001f);
			y = clip(y, 0.f, width  - 1.00001f);
			// 如果使用双线性
			if (use_bilinear) {
				// 上下界限
				const int x_low  = std::floor(x);
				const int x_high = std::min(x_low + 1, height - 1);
				const int y_low  = std::floor(y);
				const int y_high = std::min(y_low + 1, width - 1);
				// 算加权系数
				const float x_high_weight = x - x_low;
				const float x_low_weight  = 1.f - x_high_weight;
				const float y_high_weight = y - y_low;
				const float y_low_weight  = 1.f - y_high_weight;
				// 开始多通道加权
				for (int c = 0; c < channel; ++c) {
					// 找到四个原图四个点的值
					type Q1 = source[(x_low  * width + y_low)  * channel + c];
					type Q2 = source[(x_low  * width + y_high) * channel + c];
					type Q3 = source[(x_high * width + y_low)  * channel + c];
					type Q4 = source[(x_high * width + y_high) * channel + c];
					// 左右加权
					float up_value   = y_low_weight * Q1 + y_high_weight * Q2;
					float down_value = y_low_weight * Q3 + y_high_weight * Q4;
					// 上下加权
					float value = x_low_weight * up_value + x_high_weight * down_value;
					result[(i * width + j) * channel + c] = clip(value, 0, 255);
				}
			}
			// 如果是最近邻插值
			else if (use_nearest) {
				// 找到 x, y 的最近 __x, __y
				int __x = nearest_round(x);
				int __y = nearest_round(y);
				// 找到对应位置
				type* src_ptr = source + (__x * width + __y) * channel;
				type* res_ptr = result + (i   * width + j)   * channel;
				// 做多通道的赋值
				for (int c = 0; c < channel; ++c) {
					res_ptr[c] = src_ptr[c];
				}
 			}
		}
	}
}




template<typename T=float>
inline T compute_flow_intensity(const float u, const float v) {
	return std::abs(u) + std::abs(v);
}


void forward_warp_using_flow_inplementation(
		unsigned char* result, 
		unsigned char* source,
		float* flow,
		int height,
		int width,
		int channel) {
	std::vector<float> flow_intensity(height * width, 0.f);
	// 遍历每一个点
	for (int i = 0; i < height; ++i) {
		float* flow_ptr = flow + i * width * 2;
		for (int j = 0; j < width; ++j) {
			// 获取当前坐标 和 对应的光流值
			float u = flow_ptr[2 * j + 1];
			float v = flow_ptr[2 * j];
			// 找到 (i + u, j + v 最近的那个点)
			int __x = clip(nearest_round(i + u), 0.f, (height - 1) * 1.f);
			int __y = clip(nearest_round(j + v), 0.f, (width - 1)  * 1.f);
			// 先检查 (__x, __y) 位置, 是不是被赋值过, 如果本次的光流强度更大, 默认是前景, 保留前景
			int target_pos = __x * width + __y;
			float intensity = compute_flow_intensity(u, v);
			if (intensity < flow_intensity[target_pos])
				continue;
			// 本次光流更大, 覆盖之前的结果
			flow_intensity[target_pos] = intensity;
			// 给结果的这个地方 result(__x, __y) 赋值为 source(i, j), 三个通道
			unsigned char* source_start = source + (i * width + j) * channel;
			unsigned char* result_start = result + (__x * width + __y) * channel;
			for (int c = 0; c < channel; ++c) {
				result_start[c] = source_start[c];
			}
		}
	}
}







template<typename T>
inline T square(const T x) {
	return x * x;
}


/*
	这种 full 做法不行
		1. 加权半径 radius 设置多大? 不好控制
		2. 速度太慢, 不可取
		3. 加权导致 warp 结果变模糊, 丢失高频细节
*/
void full_forward_warp_using_flow_inplementation(
		unsigned char* result, 
		unsigned char* source,
		float* flow,
		int height,
		int width,
		int channel,
		int radius) {
	// 准备一个中间变量, 存储每个位置最近的所有点
	struct point {
		int x;
		int y;
		point(const int _x, const int _y)
			: x(_x), y(_y) {}
	};
	std::vector< std::list<point> > neighbors;
	neighbors.resize(height * width);
	for (int i = 0; i < height; ++i) {
		float* flow_row = flow + i * width * 2;
		for (int j = 0; j < width; ++j) {
			// 获取当前点的光流, 和移动到哪里
			float x = flow_row[2 * j + 1] + i;
			float y = flow_row[2 * j] + j;
			// 把点放到当前最近的点
			const int pos = clip(nearest_round(x), 0, height - 1) * width + clip(nearest_round(y), 0, width - 1);
			// 最近点记录移动到这里的点的值
			neighbors[pos].emplace_back(i, j);
		}
	}
	// 给定一个半径, 对于每个点, 直接去它的 3x3 邻域内找所有点加权, 比如高斯加权
	std::vector<int> offset((2 * radius + 1) * (2 * radius + 1));
	int cnt = 0;
	for (int i = -radius; i <= radius; ++i) {
		for (int j = -radius; j <= radius; ++j) {
			offset[cnt++] = i * width + j;
		}
	}
	// 生成结果的每个点
	for (int i = radius, i_end = height - radius; i < i_end; ++i) {
		// 结果指针
		unsigned char* res_ptr = result + i * width * channel;
		for (int j = radius, j_end = width - radius; j < j_end; ++j) {
			// 找到当前点的坐标
			const int pos = i * width + j;
			// 初始化
			float weight_sum = 0.f;
			// 三个通道的值
			std::vector<float> temp(channel, 0.f);
			// 找到 (i, j) 周围 3x3 窗口的所有点
			for (int k = 0; k < cnt; ++k) {
				const int Q = pos + offset[k];
				// 判断邻域 Q 有几个值, 让它的 x, y 跟 (i, j) 做距离加权
				for (const auto& it : neighbors[Q]) {
					float distance = square<float>(it.x - i) + square<float>(it.y - j);
					float weight   = std::exp(-distance / (2 * 0.81f));
					// 加权
					unsigned char* src_ptr = source + (it.x * width + it.y) * channel;
					for (int c = 0; c < channel; ++c) {
						temp[c] += weight * src_ptr[c];
					}
					weight_sum += weight;
				}
			}
			// 加权, 得到当前点的值
			for (int c = 0; c < channel; ++c) {
				res_ptr[j * channel + c] = clip<unsigned char>(temp[c] / weight_sum, 0, 255);
			}
		}
	}
}











void interp_forward_warp_using_flow_inplementation(
		unsigned char* result, 
		unsigned char* source,
		float* flow,
		int height,
		int width,
		int channel, 
		int radius) {
	// 基本过程跟原始的 forward warp 一样, 但中途需要记录哪些点被赋值了
	// 记录每一个点的光流强度, 两个点移动到同一个位置, 比较光流强度, 光流大的物体覆盖
	std::vector<float> flow_intensity(height * width, -1);
	// 遍历每一个点
	for (int i = 0; i < height; ++i) {
		float* flow_ptr = flow + i * width * 2;
		for (int j = 0; j < width; ++j) {
			// 获取当前坐标 和 对应的光流值
			float u = flow_ptr[2 * j + 1];
			float v = flow_ptr[2 * j];
			// 找到 (i + u, j + v 最近的那个点)
			int __x = clip(nearest_round(i + u), 0.f, (height - 1) * 1.f);
			int __y = clip(nearest_round(j + v), 0.f, (width - 1)  * 1.f);
			// 先检查 (__x, __y) 位置, 是不是被赋值过, 如果本次的光流强度更大, 默认是前景, 保留前景
			int target_pos = __x * width + __y;
			float intensity = compute_flow_intensity(u, v);
			if (intensity < flow_intensity[target_pos])
				continue;
			// 本次光流更大, 覆盖之前的结果
			flow_intensity[target_pos] = intensity;
			// 给结果的这个地方 result(__x, __y) 赋值为 source(i, j), 三个通道
			unsigned char* source_start = source + (i * width + j) * channel;
			unsigned char* result_start = result + (__x * width + __y) * channel;
			for (int c = 0; c < channel; ++c) {
				result_start[c] = source_start[c];
			}
		}
	}
	// 第二轮, 找到没被赋值的点, 做赋值

	// 准备上下左右四个点的偏移量
	std::vector<int> neighbor_offset({-width, -1, 1, width});
	for (int i = 1, i_end = height - 1; i < i_end; ++i) {
		for (int j = 1, j_end = width - 1; j < j_end; ++j) {
			// 获取当前位置
			const int this_pos = i * width + j;
			// 判断当前位置是否空白的
			if (flow_intensity[this_pos] < 0) {
				float weight_sum = 0.f;
				// 准备存储 rgb 的累计加权值
				std::vector<int> temp_sum(3, 0.f);
				// 遍历上下左右
				for (int k = 0; k < 4; ++k) {
					// 获取邻居点
					const int neighbor_pos = this_pos + neighbor_offset[k];
					// 如果邻居点没有值, 就不加权
					if (flow_intensity[neighbor_pos] < 0)
						continue;
					weight_sum += 1.f;
					// 这里注意, 采样来源是 result, 而不是 source
					for (int c = 0; c < channel; ++c) {
						temp_sum[c] += result[neighbor_pos * channel + c];
					}
				}
				// 写入
				unsigned char* res_ptr = result + this_pos * channel;
				for (int c = 0; c < channel; ++c) {
					res_ptr[c] = clip(temp_sum[c] / weight_sum, 0, 255);
				}
			}
		}
	}
}






template<typename S=unsigned char, typename T=float>
T compute_rgb_diff(const S* const lhs, const S* const rhs, const int length) {
	T result = 0;
	for (int i = 0; i < length; ++i) {
		result += std::abs(lhs[i] - rhs[i]);
	}
	return result;
}



void guided_forward_warp_using_flow_inplementation(
		unsigned char* result, 
		unsigned char* source,
		unsigned char* guide,
		float* flow,
		int height,
		int width,
		int channel) {
	printf("[%d %d %d]\n", height, width, channel);
	// 记录一个跟 guide 图像最接近的差异
	std::vector<float> difference(height * width, 1e8);
	// 遍历每一个点
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			// 获取 (i, j) 的光流值
			const int this_pos = i * width + j;
			const float v = flow[2 * this_pos];
			const float u = flow[2 * this_pos + 1];
			// 运动到 (i + u, j + v) 位置上, 取整
			int x = nearest_round(i + u);
			int y = nearest_round(j + v);
			// 截断在合法范围内
			x = clip<int, int>(x, 0, height - 1);
			y = clip<int, int>(y, 0, width - 1);
			const int target_pos = x * width + y;
			// 计算 source(i, j) 跟 guide(x, y) 的相似性
			float diff = compute_rgb_diff(source + this_pos * channel, guide + target_pos * channel, channel);
			// 判断运动到这里的点 source(i, j) 距离 guide(x, y) 会不会更小
			if (diff < difference[target_pos]) {
				difference[target_pos] = diff;
				// 此时可以把 source(i, j) 的点移动到 result(x, y)
				unsigned char* source_start = source + this_pos * channel;
				unsigned char* result_start = result + target_pos * channel;
				for (int c = 0; c < channel; ++c) {
					result_start[c] = source_start[c];
				}
			}
		}
	}
}




void fast_compute_occulusion_inplementation(
		unsigned char* occulusion,
		float* forward_flow, 
		float* backward_flow,
		int height, 
		int width,
		int dimension,
		float dis_threshold) {
	// 遍历每一个点
	for (int i = 0; i < height; ++i) {
		float* forward_ptr = forward_flow + i * width * dimension;
		unsigned char* occulusion_ptr = occulusion + i * width;
		for (int j = 0; j < width; ++j) {
			// 获取当前点 (i, j) 经过位移 (u, v), 移动到 (x, y)
			float x = clip(i + forward_ptr[2 * j + 1], 0.f, (height - 1) * 1.f);
			float y = clip(j + forward_ptr[2 * j],     0.f, (width  - 1) * 1.f);
			// 得到上下界限
			int x_low  = std::floor(x);
			int x_high = std::min(x_low + 1, height - 1);
			int y_low  = std::floor(y);
			int y_high = std::min(y_low + 1, width - 1);
			// 得到加权系数
			float x_high_weight = x - x_low;
			float x_low_weight  = 1.f - x_high_weight;
			float y_high_weight = y - y_low;
			float y_low_weight  = 1.f - y_high_weight;
			// 求解 (x, y) 处的反向光流, 插值得到
			float Q1 = backward_flow[(x_low * width + y_low) * dimension];
			float Q2 = backward_flow[(x_low * width + y_high) * dimension];
			float Q3 = backward_flow[(x_high * width + y_low) * dimension];
			float Q4 = backward_flow[(x_high * width + y_high) * dimension];
			float up_value   = y_low_weight * Q1 + y_high_weight * Q2;
			float down_value = y_low_weight * Q3 + y_high_weight * Q4;
			float v  = x_low_weight * up_value + x_high_weight * down_value;
			if (std::abs(y + v - j) > dis_threshold) {
				occulusion_ptr[j] = 255;
				continue;
			}
			Q1 = backward_flow[(x_low * width + y_low) * dimension + 1];
			Q2 = backward_flow[(x_low * width + y_high) * dimension + 1];
			Q3 = backward_flow[(x_high * width + y_low) * dimension + 1];
			Q4 = backward_flow[(x_high * width + y_high) * dimension + 1];
			up_value   = y_low_weight * Q1 + y_high_weight * Q2;
			down_value = y_low_weight * Q3 + y_high_weight * Q4;
			float u  = x_low_weight * up_value + x_high_weight * down_value;
			if (std::abs(x + u - i) > dis_threshold)
				occulusion_ptr[j] = 255;
		}
	}
}





// 对 mask 做一个 backward warp
void backward_warp_for_mask_inplementation(
		unsigned char* occulussion,
		unsigned char* mask,
		float* flow,
		int height,
		int width) {
	// 遍历每一个光流
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			// 当前点的光流
			float* flow_start = flow + (i * width + j) * 2;
			float u = flow_start[1];
			float v = flow_start[0];
			// 当前点, 要去 mask 的 i + u, j + v 的地方去插值
			float x = i + u;
			float y = j + v;
			// 越界的不做处理
			if (x < 0.0001f or x > height - 0.0001f or y < 0.0001f or y > width - 0.0001f)
				continue;
			// 剩下的做插值, 但是双线性不管怎么插值, 都是等于 1, 没法算遮挡
			// 上面 continue 的部分, 倒是可以算出哪些超出图像范围了
		}
	}
}



// 对 mask 做一次 forward warp
void forward_warp_for_mask_inplementation(
		unsigned char* mask,
		float* flow,
		int height,
		int width) {
	// 遍历每个光流
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			// 得到 (i, j) 的光流
			float* start = flow + (i * width + j) * 2;
			float  u     = start[1];
			float  v     = start[0];
			// 得到新位置
			float  x     = i + u;
			float  y     = j + v;
			// 判断是否越界
			if (x < 0.0001f or x > height - 1.0001f or y < 0.0001f or y > width - 1.0001f)
				continue;
			// 找到最近邻
			int  __x     = int(x + 0.49999f);
			int  __y     = int(y + 0.49999f);
			// (__x, __y) 的地方赋值为 0, 表示这里有像素来过; 抵达不了的像素, 则认为是遮挡, 保持 255
			mask[__x * width + __y] = 0;
		}
	}
}


// 编程命名接口
extern "C" {
	void backward_warp_using_flow(
		unsigned char* result, 
		unsigned char* source,
		float* flow,
		int height, 
		int width, 
		int channel,
		const char* mode="bilinear") {
		backward_warp_using_flow_inplementation<unsigned char, 2>(result, source, flow, height, width, channel, mode);
	}

	void forward_warp_using_flow(
		unsigned char* result, 
		unsigned char* source,
		float* flow,
		int height,
		int width,
		int channel) {
		forward_warp_using_flow_inplementation(result, source, flow, height, width, channel);
	}


	void full_forward_warp_using_flow(
		unsigned char* result, 
		unsigned char* source,
		float* flow,
		int height,
		int width,
		int channel, 
		int radius) {
		full_forward_warp_using_flow_inplementation(result, source, flow, height, width, channel, radius);
	}


	void interp_forward_warp_using_flow(
		unsigned char* result, 
		unsigned char* source,
		float* flow,
		int height,
		int width,
		int channel, 
		int radius) {
		interp_forward_warp_using_flow_inplementation(result, source, flow, height, width, channel, radius);
	}

	void guided_forward_warp_using_flow(
		unsigned char* result, 
		unsigned char* source,
		unsigned char* guide,
		float* flow,
		int height,
		int width,
		int channel) {
		guided_forward_warp_using_flow_inplementation(result, source, guide, flow, height, width, channel);
	}


	void fast_compute_occulusion(
		unsigned char* occulusion,
		float* forward_flow, 
		float* backward_flow,
		int height, 
		int width,
		int dimension,
		float dis_threshold) {
		fast_compute_occulusion_inplementation(occulusion, forward_flow, backward_flow, height, width, dimension, dis_threshold);
	}


	void backward_warp_for_mask(
		unsigned char* occulussion,
		unsigned char* mask,
		float* flow,
		int height,
		int width) {
		backward_warp_for_mask_inplementation(occulussion, mask, flow, height, width);
	}

	void forward_warp_for_mask(
		unsigned char* mask,
		float* flow,
		int height,
		int width) {
		forward_warp_for_mask_inplementation(mask, flow, height, width);
	}
}