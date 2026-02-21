package main

import (
	"context"
	"fmt"
	"io"
	"os"

	"github.com/minio/minio-go/v7"
	"github.com/minio/minio-go/v7/pkg/credentials"
)

func main() {
	if len(os.Args) != 8 {
		fmt.Println("Usage: miniosdktest <minio_url> <minio_access_key> <minio_secret_key> <file_name> <bucket_name> <key_name> <action>")
		fmt.Println("action: upload or download")
		os.Exit(1)
	}

	minioURL := os.Args[1]
	minioAccessKey := os.Args[2]
	minioSecretKey := os.Args[3]
	fileName := os.Args[4]
	bucketName := os.Args[5]
	keyName := os.Args[6]
	action := os.Args[7]

	// Parse endpoint and set Secure flag based on scheme
	secure := false
	endpoint := minioURL
	if len(minioURL) > 7 && minioURL[:8] == "https://" {
		secure = true
		endpoint = minioURL[8:]
	} else if len(minioURL) > 6 && minioURL[:7] == "http://" {
		secure = false
		endpoint = minioURL[7:]
	}
	minioClient, err := minio.New(endpoint, &minio.Options{
		Creds:  credentials.NewStaticV4(minioAccessKey, minioSecretKey, ""),
		Secure: secure,
	})
	if err != nil {
		fmt.Printf("Failed to create MinIO client: %v\n", err)
		os.Exit(1)
	}

	switch action {
	case "upload":
		file, err := os.Open(fileName)
		if err != nil {
			fmt.Printf("Failed to open file: %v\n", err)
			os.Exit(1)
		}
		defer file.Close()

		fileStat, err := file.Stat()
		if err != nil {
			fmt.Printf("Failed to stat file: %v\n", err)
			os.Exit(1)
		}

		_, err = minioClient.PutObject(context.TODO(), bucketName, keyName, file, fileStat.Size(), minio.PutObjectOptions{})
		if err != nil {
			fmt.Printf("Failed to upload file: %v\n", err)
			os.Exit(1)
		}

		fmt.Println("File uploaded successfully.")
	case "download":
		obj, err := minioClient.GetObject(context.TODO(), bucketName, keyName, minio.GetObjectOptions{})
		if err != nil {
			fmt.Printf("Failed to get object: %v\n", err)
			os.Exit(1)
		}
		defer obj.Close()

		outFile, err := os.Create(fileName)
		if err != nil {
			fmt.Printf("Failed to create output file: %v\n", err)
			os.Exit(1)
		}
		defer outFile.Close()

		_, err = obj.Stat()
		if err != nil {
			fmt.Printf("Failed to stat object: %v\n", err)
			os.Exit(1)
		}

		_, err = io.Copy(outFile, obj)
		if err != nil {
			fmt.Printf("Failed to download file: %v\n", err)
			os.Exit(1)
		}

		fmt.Println("File downloaded successfully.")
	default:
		fmt.Println("Unknown action. Use 'upload' or 'download'.")
		os.Exit(1)
	}
}
