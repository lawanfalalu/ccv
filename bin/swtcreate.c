#include "ccv.h"
#include <sys/time.h>
#include <ctype.h>

static int _CCV_PRINT_COUNT = 0;
static int _CCV_PRINT_LOOP = 0;

#define FLUSH(a, ...) \
	do { \
		for (_CCV_PRINT_LOOP = 0; _CCV_PRINT_LOOP < _CCV_PRINT_COUNT; _CCV_PRINT_LOOP++) \
			printf("\b"); \
		for (_CCV_PRINT_LOOP = 0; _CCV_PRINT_LOOP < _CCV_PRINT_COUNT; _CCV_PRINT_LOOP++) \
			printf(" "); \
		for (_CCV_PRINT_LOOP = 0; _CCV_PRINT_LOOP < _CCV_PRINT_COUNT; _CCV_PRINT_LOOP++) \
			printf("\b"); \
		_CCV_PRINT_COUNT = printf(a, ##__VA_ARGS__); \
		fflush(stdout); \
	} while (0) // using do while (0) to force ; line end

double ccv_swt_evaluate(int n, ccv_dense_matrix_t** images, ccv_array_t** truth, double a, ccv_swt_param_t params)
{
	int i, j, k;
	double total_f = 0, total_precision = 0, total_recall = 0;
	for (i = 0; i < n; i++)
	{
		ccv_array_t* words = ccv_swt_detect_words(images[i], params);
		double f = 0, precision = 0, recall = 0;
		for (j = 0; j < words->rnum; j++)
		{
			ccv_rect_t* estimate = (ccv_rect_t*)ccv_array_get(words, j);
			int match = 0;
			for (k = 0; k < truth[i]->rnum; k++)
			{
				ccv_rect_t* target = (ccv_rect_t*)ccv_array_get(truth[i], k);
				match = ccv_max(match, ccv_max(ccv_min(target->x + target->width, estimate->x + estimate->width) - ccv_max(target->x, estimate->x), 0) * ccv_max(ccv_min(target->y + target->height, estimate->y + estimate->height) - ccv_max(target->y, estimate->y), 0));
			}
			precision += (double)match / (double)(estimate->width * estimate->height);
		}
		if (words->rnum > 0)
			precision /= words->rnum;
		for (j = 0; j < truth[i]->rnum; j++)
		{
			ccv_rect_t* target = (ccv_rect_t*)ccv_array_get(truth[i], j);
			int match = 0;
			for (k = 0; k < words->rnum; k++)
			{
				ccv_rect_t* estimate = (ccv_rect_t*)ccv_array_get(words, k);
				match = ccv_max(match, ccv_max(ccv_min(target->x + target->width, estimate->x + estimate->width) - ccv_max(target->x, estimate->x), 0) * ccv_max(ccv_min(target->y + target->height, estimate->y + estimate->height) - ccv_max(target->y, estimate->y), 0));
			}
			recall += (double)match / (double)(target->width * target->height);
		}
		ccv_array_free(words);
		if (truth[i]->rnum > 0)
			recall /= truth[i]->rnum;
		if (precision > 0 && recall > 0)
			f = 1 / (a / precision + (1 - a) / recall);
		total_f += f;
		total_precision += precision;
		total_recall += recall;
	}
	total_f /= n;
	total_precision /= n;
	total_recall /= n;
	return total_f;
}

int main(int argc, char** argv)
{
	FILE* r = fopen(argv[1], "rt");
	if (argc == 3)
		chdir(argv[2]);
	int images;
	fscanf(r, "%d", &images);
	int i;
	ccv_enable_default_cache();
	ccv_dense_matrix_t** aof = (ccv_dense_matrix_t**)ccmalloc(sizeof(ccv_dense_matrix_t*) * images);
	ccv_array_t** aow = (ccv_array_t**)ccmalloc(sizeof(ccv_array_t**) * images);
	for (i = 0; i < images; i++)
	{
		char file[1000];
		fscanf(r, "%s", file);
		aof[i] = 0;
		ccv_read(file, aof + i, CCV_IO_GRAY | CCV_IO_ANY_FILE);
		int locations;
		fscanf(r, "%d", &locations);
		int j;
		aow[i] = ccv_array_new(sizeof(ccv_rect_t), locations, 0);
		for (j = 0; j < locations; j++)
		{
			double x, y, width, height;
			fscanf(r, "%lf %lf %lf %lf", &x, &y, &width, &height);
			ccv_rect_t rect = { .x = (int)x, .y = (int)y, .width = (int)width, .height = (int)height };
			ccv_array_push(aow[i], &rect);
		}
	}
	ccv_swt_param_t params = {
		.size = 3,
		.low_thresh = 76,
		.high_thresh = 228,
		.max_height = 300,
		.min_height = 10,
		.aspect_ratio = 10,
		.variance_ratio = 0.6,
		.thickness_ratio = 1.5,
		.height_ratio = 3,
		.intensity_thresh = 26,
		.distance_ratio = 3,
		.intersect_ratio = 2,
		.letter_thresh = 3,
		.elongate_ratio = 1.6,
		.breakdown = 1,
		.breakdown_ratio = 0.78,
	};
	double best_f = 0;
	ccv_swt_param_t best_params = params;
	for (i = 50; i < 150; i++)
	{
		params.low_thresh = i;
		params.high_thresh = i * 3;
		double f = ccv_swt_evaluate(images, aof, aow, 0.5, params);
		if (f > best_f)
		{
			best_params = params;
			best_f = f;
		}
		FLUSH("current f : %lf, best f : %lf, at low_thresh = %d", f, best_f, best_params.low_thresh);
	}
	printf("\n");
	params = best_params;
	for (i = params.low_thresh * 2; i < params.low_thresh * 4; i++)
	{
		params.high_thresh = i;
		double f = ccv_swt_evaluate(images, aof, aow, 0.5, params);
		if (f > best_f)
		{
			best_params = params;
			best_f = f;
		}
		FLUSH("current f : %lf, best f : %lf, at high_thresh = %d", f, best_f, best_params.high_thresh);
	}
	printf("\n");
	params = best_params;
	for (i = 5; i < 30; i++)
	{
		params.intensity_thresh = i;
		double f = ccv_swt_evaluate(images, aof, aow, 0.5, params);
		if (f > best_f)
		{
			best_params = params;
			best_f = f;
		}
		FLUSH("current f : %lf, best f : %lf, at intensity_thresh = %d", f, best_f, best_params.intensity_thresh);
	}
	printf("\n");
	params = best_params;
	for (i = 1; i <= 30; i++)
	{
		params.variance_ratio = i / 10.0;
		double f = ccv_swt_evaluate(images, aof, aow, 0.5, params);
		if (f > best_f)
		{
			best_params = params;
			best_f = f;
		}
		FLUSH("current f : %lf, best f : %lf, at variance_ratio = %lf", f, best_f, best_params.variance_ratio);
	}
	printf("\n");
	params = best_params;
	for (i = 1; i <= 100; i++)
	{
		params.elongate_ratio = i / 10.0;
		double f = ccv_swt_evaluate(images, aof, aow, 0.5, params);
		if (f > best_f)
		{
			best_params = params;
			best_f = f;
		}
		FLUSH("current f : %lf, best f : %lf, at elongate_ratio = %lf", f, best_f, best_params.elongate_ratio);
	}
	printf("\n");
	params = best_params;
	for (i = 1; i <= 150; i++)
	{
		params.breakdown_ratio = i / 100.0;
		double f = ccv_swt_evaluate(images, aof, aow, 0.5, params);
		if (f > best_f)
		{
			best_params = params;
			best_f = f;
		}
		FLUSH("current f : %lf, best f : %lf, at breakdown_ratio = %lf", f, best_f, best_params.breakdown_ratio);
	}
	printf("\nbest parameters for swt is:\n\tlow_thresh = %d\n\thigh_thresh = %d\n\tintensity_thresh = %d\n\tvariance_ratio = %lf\n\telongate_ratio = %lf\n\tbreakdown_ratio = %lf\n", best_params.low_thresh, best_params.high_thresh, best_params.intensity_thresh, best_params.variance_ratio, best_params.elongate_ratio, best_params.breakdown_ratio);
	ccfree(aof);
	ccfree(aow);
	ccv_disable_cache();
	return 0;
}
