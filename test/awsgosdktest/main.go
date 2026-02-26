package main

import (
	"context"
	"fmt"
	"os"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/credentials"
	"github.com/aws/aws-sdk-go-v2/service/s3"
	s3types "github.com/aws/aws-sdk-go-v2/service/s3/types"
)

func main() {
	if len(os.Args) != 8 {
		fmt.Println("Usage: gosdktest <aws_url> <aws_access_key> <aws_secret_key> <file_name> <bucket_name> <key_name> <action>")
		fmt.Println("action: upload or download")
		os.Exit(1)
	}

	awsURL := os.Args[1]
	awsAccessKey := os.Args[2]
	awsSecretKey := os.Args[3]
	fileName := os.Args[4]
	bucketName := os.Args[5]
	keyName := os.Args[6]
	action := os.Args[7]

	cfg, err := config.LoadDefaultConfig(context.TODO(),
		config.WithCredentialsProvider(credentials.NewStaticCredentialsProvider(awsAccessKey, awsSecretKey, "")),
		config.WithRegion("us-east-1"),
		config.WithEndpointResolver(
			aws.EndpointResolverFunc(func(service, region string) (aws.Endpoint, error) {
				return aws.Endpoint{
					URL: awsURL,
				}, nil
			}),
		),
	)
	if err != nil {
		fmt.Printf("Failed to load AWS config: %v\n", err)
		os.Exit(1)
	}

	s3Client := s3.NewFromConfig(cfg, func(o *s3.Options) {
		o.UsePathStyle = true
	})

	switch action {
	case "upload":
		file, err := os.Open(fileName)
		if err != nil {
			fmt.Printf("Failed to open file: %v\n", err)
			os.Exit(1)
		}
		defer file.Close()

		_, err = s3Client.PutObject(context.TODO(), &s3.PutObjectInput{
			Bucket:            &bucketName,
			Key:               &keyName,
			Body:              file,
			ChecksumAlgorithm: s3types.ChecksumAlgorithmSha256,
		})
		if err != nil {
			fmt.Printf("Failed to upload file: %v\n", err)
			os.Exit(1)
		}

		fmt.Println("File uploaded successfully.")
	case "download":
		getObjOutput, err := s3Client.GetObject(context.TODO(), &s3.GetObjectInput{
			Bucket: &bucketName,
			Key:    &keyName,
		})
		if err != nil {
			fmt.Printf("Failed to get object: %v\n", err)
			os.Exit(1)
		}
		defer getObjOutput.Body.Close()

		outFile, err := os.Create(fileName)
		if err != nil {
			fmt.Printf("Failed to create output file: %v\n", err)
			os.Exit(1)
		}
		defer outFile.Close()

		_, err = outFile.ReadFrom(getObjOutput.Body)
		if err != nil {
			fmt.Printf("Failed to download file: %v\n", err)
			os.Exit(1)
		}

		fmt.Println("File downloaded successfully.")
	case "listbuckets":
		listBucketsOutput, err := s3Client.ListBuckets(context.TODO(), &s3.ListBucketsInput{})
		if err != nil {
			fmt.Printf("Failed to list buckets: %v\n", err)
			os.Exit(1)
		}

		containsBucket := false
		fmt.Println("Buckets:")
		for _, bucket := range listBucketsOutput.Buckets {
			fmt.Printf("- %s - created: %v location: %s\n", aws.ToString(bucket.Name), aws.ToTime(bucket.CreationDate), aws.ToString(bucket.BucketRegion))
			if aws.ToString(bucket.Name) == bucketName {
				containsBucket = true
			}
		}

		if !containsBucket {
			fmt.Printf("Bucket '%s' not found in the list.\n", bucketName)
			os.Exit(1)
		}

	default:
		fmt.Println("Unknown action. Use 'upload' or 'download'.")
		os.Exit(1)
	}
}
