/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Copyright (c) 2012, Sergey Lisitsyn
 */

#ifndef TAPKEE_METHODS_H_
#define TAPKEE_METHODS_H_

#include "defines.hpp"
#include "methods/locally_linear.hpp"
#include "methods/eigen_embedding.hpp"
#include "methods/generalized_eigen_embedding.hpp"
#include "methods/multidimensional_scaling.hpp"
#include "methods/diffusion_maps.hpp"
#include "methods/laplacian_eigenmaps.hpp"
#include "methods/isomap.hpp"
#include "methods/pca.hpp"
#include "methods/spe.hpp"
#include "neighbors/neighbors.hpp"

template <class RandomAccessIterator, class KernelCallback, class DistanceCallback, class FeatureVectorCallback, int>
struct embedding_impl
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback kernel_callback, DistanceCallback distance_callback,
                          FeatureVectorCallback feature_vector_callback, ParametersMap options);
};

#define CONCRETE_IMPLEMENTATION(METHOD) \
	template <class RandomAccessIterator, class KernelCallback, class DistanceCallback, class FeatureVectorCallback> \
	struct embedding_impl<RandomAccessIterator,KernelCallback,DistanceCallback,FeatureVectorCallback,METHOD>
#define OBTAIN_PARAMETER(TYPE,NAME,CODE) \
	TYPE NAME = options[CODE].cast<TYPE>()
#define SKIP_ONE_EIGENVALUE 1
#define SKIP_NO_EIGENVALUES 0

CONCRETE_IMPLEMENTATION(KERNEL_LOCALLY_LINEAR_EMBEDDING)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback kernel_callback, DistanceCallback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(DefaultScalarType,eigenshift,EIGENSHIFT);

		timed_context context("Embedding with KLLE");
		Neighbors neighbors =
			find_neighbors(neighbors_method,begin,end,kernel_callback,k);
		SparseWeightMatrix weight_matrix =
			klle_weight_matrix(begin,end,neighbors,kernel_callback,eigenshift);
		return eigen_embedding<SparseWeightMatrix,InverseSparseMatrixOperation>(eigen_method,
			weight_matrix,target_dimension,SKIP_ONE_EIGENVALUE);
	}
};

CONCRETE_IMPLEMENTATION(KERNEL_LOCAL_TANGENT_SPACE_ALIGNMENT)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback kernel_callback, DistanceCallback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(DefaultScalarType,eigenshift,EIGENSHIFT);
		
		timed_context context("Embedding with KLTSA");
		Neighbors neighbors = 
			find_neighbors(neighbors_method,begin,end,kernel_callback,k);
		SparseWeightMatrix weight_matrix = 
			kltsa_weight_matrix(begin,end,neighbors,kernel_callback,target_dimension,eigenshift);
		return eigen_embedding<SparseWeightMatrix,InverseSparseMatrixOperation>(eigen_method,
			weight_matrix,target_dimension,SKIP_ONE_EIGENVALUE);
	}
};

CONCRETE_IMPLEMENTATION(DIFFUSION_MAP)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback distance_callback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(unsigned int,timesteps,DIFFUSION_MAP_TIMESTEPS);
		OBTAIN_PARAMETER(DefaultScalarType,width,GAUSSIAN_KERNEL_WIDTH);
		
		timed_context context("Embedding with diffusion map");
		DenseSymmetricMatrix diffusion_matrix =
			compute_diffusion_matrix(begin,end,distance_callback,timesteps,width);
		return eigen_embedding<DenseSymmetricMatrix, DenseImplicitSquareMatrixOperation>(eigen_method,
			diffusion_matrix,target_dimension,SKIP_NO_EIGENVALUES);
	}
};

CONCRETE_IMPLEMENTATION(MULTIDIMENSIONAL_SCALING)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback distance_callback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);

		timed_context context("Embeding with MDS");
		DenseSymmetricMatrix distance_matrix = compute_distance_matrix(begin,end,distance_callback);
		mds_process_matrix(distance_matrix);
		return eigen_embedding<DenseSymmetricMatrix,DenseMatrixOperation>(eigen_method,
			distance_matrix,target_dimension,SKIP_NO_EIGENVALUES);
	}
};

CONCRETE_IMPLEMENTATION(LANDMARK_MULTIDIMENSIONAL_SCALING)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback distance_callback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(DefaultScalarType,ratio,LANDMARK_RATIO);

		timed_context context("Embedding with Landmark MDS");
		Landmarks landmarks = 
			select_landmarks_random(begin,end,ratio);
		DenseSymmetricMatrix distance_matrix = 
			compute_distance_matrix(begin,landmarks,distance_callback);
		mds_process_matrix(distance_matrix);
		EmbeddingResult landmarks_embedding = 
			eigen_embedding<DenseSymmetricMatrix,DenseMatrixOperation>(eigen_method,
					distance_matrix,target_dimension,SKIP_NO_EIGENVALUES);
		return triangulate(begin,end,distance_callback,landmarks,
			distance_matrix,landmarks_embedding,target_dimension);
	}
};

CONCRETE_IMPLEMENTATION(ISOMAP)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback distance_callback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);

		timed_context context("Embedding with Isomap");
		Neighbors neighbors = 
			find_neighbors(neighbors_method,begin,end,distance_callback,k);
		DenseSymmetricMatrix shortest_distances_matrix = 
			compute_shortest_distances_matrix(begin,end,neighbors,distance_callback);
		return eigen_embedding<DenseSymmetricMatrix,DenseMatrixOperation>(eigen_method,
			shortest_distances_matrix,target_dimension,SKIP_NO_EIGENVALUES);
	}
};

CONCRETE_IMPLEMENTATION(NEIGHBORHOOD_PRESERVING_EMBEDDING)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback kernel_callback, DistanceCallback,
                          FeatureVectorCallback feature_vector_callback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		OBTAIN_PARAMETER(unsigned int,dimension,CURRENT_DIMENSION);
		OBTAIN_PARAMETER(DefaultScalarType,eigenshift,EIGENSHIFT);
		
		timed_context context("Embedding with NPE");
		Neighbors neighbors = 
			find_neighbors(neighbors_method,begin,end,kernel_callback,k);
		SparseWeightMatrix weight_matrix = 
			klle_weight_matrix(begin,end,neighbors,kernel_callback,eigenshift);
		DenseSymmetricMatrixPair eig_matrices =
			construct_neighborhood_preserving_eigenproblem(weight_matrix,begin,end,
				feature_vector_callback,dimension);
		ProjectionResult projection_result = 
			generalized_eigen_embedding<DenseSymmetricMatrix,DenseSymmetricMatrix,DenseMatrixOperation>(
				eigen_method,eig_matrices.first,eig_matrices.second,target_dimension,SKIP_NO_EIGENVALUES);
		// TODO to be improved with out-of-sample projection
		return project(projection_result,begin,end,feature_vector_callback,dimension);
	}
};

CONCRETE_IMPLEMENTATION(HESSIAN_LOCALLY_LINEAR_EMBEDDING)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback kernel_callback, DistanceCallback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		
		timed_context context("Embedding with HLLE");
		Neighbors neighbors =
			find_neighbors(neighbors_method,begin,end,kernel_callback,k);
		SparseWeightMatrix weight_matrix =
			hlle_weight_matrix(begin,end,neighbors,kernel_callback,target_dimension);
		return eigen_embedding<SparseWeightMatrix,InverseSparseMatrixOperation>(eigen_method,
			weight_matrix,target_dimension,SKIP_ONE_EIGENVALUE);
	}
};

CONCRETE_IMPLEMENTATION(LAPLACIAN_EIGENMAPS)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback distance_callback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		OBTAIN_PARAMETER(DefaultScalarType,width,GAUSSIAN_KERNEL_WIDTH);
		
		timed_context context("Embedding with Laplacian Eigenmaps");
		Neighbors neighbors = 
			find_neighbors(neighbors_method,begin,end,distance_callback,k);
		Laplacian laplacian = 
			compute_laplacian(begin,end,neighbors,distance_callback,width);
		return generalized_eigen_embedding<SparseWeightMatrix,DenseSymmetricMatrix,InverseSparseMatrixOperation>(
			eigen_method,laplacian.first,laplacian.second,target_dimension,SKIP_ONE_EIGENVALUE);
	}
};

CONCRETE_IMPLEMENTATION(LOCALITY_PRESERVING_PROJECTIONS)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback distance_callback,
                          FeatureVectorCallback feature_vector_callback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		OBTAIN_PARAMETER(DefaultScalarType,width,GAUSSIAN_KERNEL_WIDTH);
		OBTAIN_PARAMETER(unsigned int,dimension,CURRENT_DIMENSION);
		
		timed_context context("Embedding with LPP");
		Neighbors neighbors = 
			find_neighbors(neighbors_method,begin,end,distance_callback,k);
		Laplacian laplacian = 
			compute_laplacian(begin,end,neighbors,distance_callback,width);
		DenseSymmetricMatrixPair eigenproblem_matrices =
			construct_locality_preserving_eigenproblem(laplacian.first,laplacian.second,begin,end,
					feature_vector_callback,dimension);
		ProjectionResult projection_result = 
			generalized_eigen_embedding<DenseSymmetricMatrix,DenseSymmetricMatrix,DenseMatrixOperation>(
				eigen_method,eigenproblem_matrices.first,eigenproblem_matrices.second,target_dimension,SKIP_NO_EIGENVALUES);
		// TODO to be improved with out-of-sample projection
		return project(projection_result,begin,end,feature_vector_callback,dimension);
	}
};

CONCRETE_IMPLEMENTATION(PCA)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback,
                          FeatureVectorCallback feature_vector_callback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,dimension,CURRENT_DIMENSION);
		
		timed_context context("Embedding with PCA");
		DenseSymmetricMatrix centered_covariance_matrix = 
			compute_covariance_matrix(begin,end,feature_vector_callback,dimension);
		ProjectionResult projection_result = 
			eigen_embedding<DenseSymmetricMatrix,DenseMatrixOperation>(eigen_method,centered_covariance_matrix,target_dimension,SKIP_NO_EIGENVALUES);
		// TODO to be improved with out-of-sample projection
		return project(projection_result,begin,end,feature_vector_callback,dimension);
	}
};

CONCRETE_IMPLEMENTATION(KERNEL_PCA)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback kernel_callback, DistanceCallback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);

		timed_context context("Embedding with kPCA");
		DenseSymmetricMatrix centered_kernel_matrix = 
			compute_centered_kernel_matrix(begin,end,kernel_callback);
		return eigen_embedding<DenseSymmetricMatrix,DenseMatrixOperation>(eigen_method,
			centered_kernel_matrix,target_dimension,SKIP_NO_EIGENVALUES);
	}
};

CONCRETE_IMPLEMENTATION(LINEAR_LOCAL_TANGENT_SPACE_ALIGNMENT)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback kernel_callback, DistanceCallback,
                          FeatureVectorCallback feature_vector_callback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(TAPKEE_EIGEN_EMBEDDING_METHOD,eigen_method,EIGEN_EMBEDDING_METHOD);
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		OBTAIN_PARAMETER(unsigned int,dimension,CURRENT_DIMENSION);
		OBTAIN_PARAMETER(DefaultScalarType,eigenshift,EIGENSHIFT);
		
		timed_context context("Embedding with LLTSA");
		Neighbors neighbors = 
			find_neighbors(neighbors_method,begin,end,kernel_callback,k);
		SparseWeightMatrix weight_matrix = 
			kltsa_weight_matrix(begin,end,neighbors,kernel_callback,target_dimension,eigenshift);
		DenseSymmetricMatrixPair eig_matrices =
			construct_lltsa_eigenproblem(weight_matrix,begin,end,
				feature_vector_callback,dimension);
		ProjectionResult projection_result = 
			generalized_eigen_embedding<DenseSymmetricMatrix,DenseSymmetricMatrix,DenseMatrixOperation>(
				eigen_method,eig_matrices.first,eig_matrices.second,target_dimension,SKIP_NO_EIGENVALUES);
		// TODO to be improved with out-of-sample projection
		return project(projection_result,begin,end,feature_vector_callback,dimension);
	}
};

CONCRETE_IMPLEMENTATION(STOCHASTIC_PROXIMITY_EMBEDDING)
{
	EmbeddingResult embed(RandomAccessIterator begin, RandomAccessIterator end,
                          KernelCallback, DistanceCallback distance_callback,
                          FeatureVectorCallback, ParametersMap options)
	{
		OBTAIN_PARAMETER(unsigned int,target_dimension,TARGET_DIMENSION);
		OBTAIN_PARAMETER(unsigned int,k,NUMBER_OF_NEIGHBORS);
		OBTAIN_PARAMETER(TAPKEE_NEIGHBORS_METHOD,neighbors_method,NEIGHBORS_METHOD);
		OBTAIN_PARAMETER(unsigned int,dimension,CURRENT_DIMENSION);
		OBTAIN_PARAMETER(bool,global_strategy,SPE_GLOBAL_STRATEGY);
		OBTAIN_PARAMETER(DefaultScalarType,tolerance,SPE_TOLERANCE);
		OBTAIN_PARAMETER(unsigned int,nupdates,SPE_NUM_UPDATES);

		//TODO add local strategy using KNN
		if (!global_strategy)
		{
			printf("Local strategy in SPE not implemented yet\n");
			return EmbeddingResult();
		}

		timed_context context("Embedding with SPE");
		return spe_embedding(begin,end,distance_callback,k,target_dimension,global_strategy,tolerance,nupdates);
	}
};

#undef CONCRETE_IMPLEMENTATION
#undef OBTAIN_PARAMETER
#endif