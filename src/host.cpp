#include <algorithm>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "ntt.h"


int bit_reverse(int i){
	ap_uint<log2N> x = (ap_uint<log2N>) i;
	ap_uint<log2N> reversed_idx = x.reverse();
	return (int) reversed_idx;
}

int mod_power(int x, int exp, int mod){
    int result = 1;
    for(int i = 0; i < exp; i++){
        result *= x;
        result %= mod;
    }
    return result;

}

void get_omega_mat(int Omega[n][n], int mod, int psi) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            int exp = (2*(i*j)+j) % (2*n);
            Omega[i][j] = mod_power(psi, exp, mod);
        }
    }
}


void sw_ntt(std::vector<Data, tapa::aligned_allocator<Data>> A, std::vector<Data, tapa::aligned_allocator<Data>> &out_sw, int psi, int q, const int SAMPLES){
  int omega[n][n];
  get_omega_mat(omega, mod, psi);
  #pragma omp parallel
	{
		int tid = omp_get_thread_num();
		if( tid == 0 ){
			int nthreads = omp_get_num_threads();
			// std::cout << "Running OpenMP with " << nthreads << " threads...\n";
		}
	}
  #pragma omp parallel for
  for(int i = 0; i < SAMPLES; i++){
    for (int j = 0; j < n; ++j) {
        int sum = 0;
        for (int k = 0; k < n; ++k) {
            sum = (sum + omega[j][k] * A[n*i+k]) % q;
        }
        out_sw[n*i + j] = sum;
    }
  }
}

void bit_reverse_hw_out(std::vector<Data, tapa::aligned_allocator<Data>> out_hw, std::vector<Data, tapa::aligned_allocator<Data>> &out_hw_BR, const int SAMPLES){
#pragma omp parallel 
{
    int tid = omp_get_thread_num();
    if( tid == 0 ){
            int nthreads = omp_get_num_threads();
            // std::cout << "Running OpenMP with " << nthreads << " threads...\n";
    }
}
#pragma omp parallel for
//Bit-reversing the HW output
    for(int i = 0; i < SAMPLES; i++){
      for(int j = 0; j < n; j++){
          out_hw_BR[n*i + bit_reverse(j)] = out_hw[n*i + j];
      }
    }
}

void ntt_ct_temporal(std::vector<Data, tapa::aligned_allocator<Data>> input, std::vector<Data, tapa::aligned_allocator<Data>> &output, int SAMPLES){
    
    int coeff[log2N+1][n];

    for(int ind = 0; ind < SAMPLES; ind++){
        
        for(int i = 0; i < n; i++){
          coeff[0][i] = input[n*ind + i];
        }

        for (size_t stage = 0; stage < log2N - log2B -1; stage++)
        {
            int stride = (n >> (stage + 1));

            for (size_t i = 0; i < n / 2; i++)
            {
                int idx_even = (i % stride) + (i / stride) * 2 * stride;
                int idx_odd = idx_even + stride;
                int idx_psi = (i / stride) + n / (2 * stride);

                int multiplied = coeff[stage][idx_odd] * tw_factors[idx_psi];

                int t = multiplied % mod;

                // Cooley-Tukey butterfly
                int coeff_odd = coeff[stage][idx_even] - t;
                int coeff_even = coeff[stage][idx_even] + t;

                // Write results to temp variables first to minimize dependencies
                int temp_odd = (coeff_odd < 0) ? (coeff_odd + mod) : coeff_odd;
                int temp_even = (coeff_even >= mod) ? (coeff_even - mod) : coeff_even;

                coeff[stage+1][idx_odd] = temp_odd;
                coeff[stage+1][idx_even] = temp_even;
            }
                
        }

        for(int i = 0; i < n; i++){
            output[n*ind + i] = coeff[log2N - log2B -1][i];
        }    
    }
}

void rearrange(std::vector<Data, tapa::aligned_allocator<Data>> arr, std::vector<Data, tapa::aligned_allocator<Data>> &out, int last_stride, int SAMPLES){
    int stride = last_stride >> 1;

    for(int ind = 0; ind < SAMPLES; ind++){
      for(int i = 0; i < n/last_stride; i++){
        for(int j = 0; j < stride; j++){
            out[n*ind + i*last_stride+ j] = arr[n*ind + i*last_stride+ 2*j];
            out[n*ind + i*last_stride+ j + stride] = arr[n*ind + i*last_stride+ 2*j+1];
        }
      }
    }
    
}

void print_array(std::vector<Data, tapa::aligned_allocator<Data>> arr, int SAMPLES){

    for(int i = 0; i < SAMPLES; i++){
      for(int j = 0; j < DEPTH; j++){
            for(int k = 0; k < WIDTH; k++){          
            std::cout << arr[n*i + WIDTH*j + k] << " ";
            }
            std::cout << std::endl;
  		}
    }
}



void ntt(tapa::mmaps<bits<DataVec>, NUM_CH> x, tapa::mmaps<bits<DataVec>, NUM_CH> y, int SAMPLES);

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {

    gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

    const int SAMPLES = argc > 1 ? atoll(argv[1]) : 1000;

    const int N = n;
    

    std::vector<Data, tapa::aligned_allocator<Data>> input(n*SAMPLES);
    std::vector<Data, tapa::aligned_allocator<Data>> out_sw(n*SAMPLES);
    std::vector<Data, tapa::aligned_allocator<Data>> out_hw(n*SAMPLES);
    std::vector<Data, tapa::aligned_allocator<Data>> out_hw_BR(n*SAMPLES);

    std::vector<std::vector<Data, tapa::aligned_allocator<Data>>> X(NUM_CH);
    std::vector<std::vector<Data, tapa::aligned_allocator<Data>>> Y(NUM_CH);



    int last_stride = n >> (log2N - log2B -1);

    // Generate twiddle factors
    std::cout << "n: " << n << " mod: " << mod << std::endl;
    std::cout << "NUMBER OF SAMPLES: " << SAMPLES << std::endl; 
    std::cout << "Number of butterfly units in parallel: " << B << std::endl; 
    std::cout << "Number of input Memory channels: " << NUM_CH << std::endl;
    std::cout << "Number of NTT cores: " << NUM_CORE << std::endl;

  
    srand(time(NULL));

    // Create the test data 
    for(int i = 0; i < SAMPLES; i++){
      for(int j = 0; j < n; j++){
        //   input[i*n + j] = rand() % mod;
          input[i*n + j] = (i*n + j) % mod;
      }
    }



    // Resize each channel into appropriate size
    int CHANNEL_SIZE = SAMPLES*n/NUM_CH;

    for (int cc = 0; cc < NUM_CH; ++cc) {
        X[cc].resize(CHANNEL_SIZE, 0);
        Y[cc].resize(CHANNEL_SIZE, 0);
    }


    for (int i = 0; i < SAMPLES/NUM_CORE; i++) {
        for(int x = 0; x < NUM_CORE; x++){

            // NTT_CORE LOOP
            for (int j = 0; j < N / (NUM_CH_PER_CORE*kVecLen); j++) {       
                for (int y = 0; y < NUM_CH_PER_CORE; y++){
                    
                    int cc = NUM_CH_PER_CORE*x + y; // Memory Channel

                    for (int k = 0; k < (kVecLen/2); k++) {
                        
                        int idx = i * N / NUM_CH_PER_CORE + j * kVecLen  + k;                           
                        int input_base_idx = N * (NUM_CORE*i + x) + (kVecLen/2) *(NUM_CH_PER_CORE*j + y) + k;

                        X[cc][idx] = input[input_base_idx];  
                        X[cc][idx + kVecLen/2] = input[input_base_idx + n / 2];  

                    }

                    // for(int k = 0; k < kVecLen; k++){
                    //     int idx = i * N / NUM_CH_PER_CORE + j * kVecLen  + k;   
                    //     int input_base_idx = N * (NUM_CORE*i + x) + (kVecLen) *(NUM_CH_PER_CORE*j + y) + k;
                        
                    //     X[cc][idx] = input[input_base_idx]; 
                    // }
                    
                    // for(int k = 0; k < kVecLen; k++){
                    //     int idx = i * N / (NUM_CH_PER_CORE*kVecLen) + j * kVecLen  + k;   
                    //     std::cout << X[cc][idx] << " ";
                    // }
                    // std::cout << std::endl;
                }   
            }
            
        }
    }
    
    std::cout << "Test data generation DONE" << std::endl;


    // ntt_ct_temporal(input, out_sw, SAMPLES);
    sw_ntt(input, out_sw, psi, mod, SAMPLES);

    std::cout << "SW Computation Done" << std::endl;

    int64_t kernel_time_ns =
      tapa::invoke(ntt, FLAGS_bitstream,
                   tapa::read_only_mmaps<Data, NUM_CH>(X).reinterpret<bits<DataVec>>(),
                   tapa::write_only_mmaps<Data, NUM_CH>(Y).reinterpret<bits<DataVec>>(), SAMPLES/NUM_CORE);


    std::clog << "kernel time: " << kernel_time_ns * 1e-9 << " s" << std::endl;
    std::clog << "kernel time: " << kernel_time_ns * 1e-6  << " ms" << std::endl;
    std::clog << "kernel time: " << kernel_time_ns * 1e-3 << " us" << std::endl;


    for (int i = 0; i < SAMPLES/NUM_CORE; i++) {
        for(int x = 0; x < NUM_CORE; x++){
            
            int sample_idx = NUM_CORE*i + x;
            
            // NTT_CORE LOOP
            for (int j = 0; j < N / (NUM_CH_PER_CORE*kVecLen); j++) {       
                for (int y = 0; y < NUM_CH_PER_CORE; y++){

                    int depth_idx = NUM_CH_PER_CORE*j + y;
                    
                    int cc = NUM_CH_PER_CORE*x + y; // Memory Channel

                    for (int k = 0; k < kVecLen; k++) {
                        
                        int idx = i * N / NUM_CH_PER_CORE + j * kVecLen  + k;   
                        int out_hw_idx =  N*sample_idx +  kVecLen *depth_idx + k;

                        out_hw[out_hw_idx] = Y[cc][idx];   
                    }
                }   
            }
            
        }
    }
    

    
    std::cout << "Rearranging ...\n";
    bit_reverse_hw_out(out_hw, out_hw_BR, SAMPLES);
    std::cout << "Done\n";
        
    // Printing
    // for(int i = 0; i < SAMPLES; i++){
    //  std::cout << "INPUT" << std::endl;
    //  for(int j = 0; j < n / WIDTH; j++){
    //      for(int k = 0; k < WIDTH; k++){
    //          std::cout << input[n * i + WIDTH * j + k] << " ";
    //      }
    //      std::cout << std::endl;
    //  }
    //  std::cout << std::endl;

    //  std::cout << "SW OUTPUT" << std::endl;
    //  for(int j = 0; j < n / WIDTH; j++){
    //      for(int k = 0; k < WIDTH; k++){
    //          std::cout << out_sw[n * i + WIDTH * j + k] << " ";
    //      }
    //      std::cout << std::endl;
    //  }
    //  std::cout << std::endl;

    //  std::cout << "HW OUTPUT" << std::endl;
    //  for(int j = 0; j < n / WIDTH; j++){
    //      for(int k = 0; k < WIDTH; k++){
    //          std::cout << out_hw[n * i + WIDTH * j + k] << " ";
    //      }
    //      std::cout << std::endl;
    //  }
    //  std::cout << std::endl;

    //  std::cout << "HW OUTPUT (BR)" << std::endl;
    //  for(int j = 0; j < n / WIDTH; j++){
    //     for(int k = 0; k < WIDTH; k++){
    //         std::cout << out_hw_BR[n * i + WIDTH * j + k] << " ";
    //     }
    //     std::cout << std::endl;
    //  }
    //  std::cout << std::endl;
    // }



    // Compare the results of the Device to the simulation
    int err_cnt = 0;
    for(int i = 0; i<SAMPLES; i++){
      for(int j = 0; j< n; j++){
          if(out_sw[i*n+j] != out_hw_BR[i*n+j]) {
              err_cnt++;
          }
      }
    }

    if(err_cnt != 0){
		  printf("FAILED! Error count : %d\n", err_cnt);
  	}
  	else{
  		printf("PASSED!\n");
  	}
  
  	return EXIT_SUCCESS;
}
