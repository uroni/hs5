// This file is auto-generated by @hey-api/openapi-ts


export type $OpenApiTs = {
    '/api-v1-b64be512-4b03-4028-a589-13931942e205/login': {
        post: {
            req: {
                requestBody?: {
                    username: string;
                    password: string;
                };
            };
            res: {
                /**
                 * OK
                 */
                200: {
                    ses: string;
                };
            };
        };
    };
    '/api-v1-b64be512-4b03-4028-a589-13931942e205/adduser': {
        post: {
            req: {
                requestBody?: {
                    ses: string;
                    username: string;
                    password: string;
                };
            };
            res: {
                /**
                 * OK
                 */
                200: {
                    dummy?: string;
                };
            };
        };
    };
};