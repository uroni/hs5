import { FormEvent, useState } from "react";
import { TableWrapper } from "../components/TableWrapper";
import { Button, Field, Input, Spinner } from "@fluentui/react-components";
import { AddRegular } from "@fluentui/react-icons";
import { Pages, router, state } from "../App";
import { ApiError, postApiV1B64Be5124B034028A58913931942E205AddBucket } from "../api";
import { HapiError, Herror } from "../errorapi/HapiError";
import { useQueryClient } from "@tanstack/react-query";


const AddBucket = () => {
    const queryClient = useQueryClient();
    const [isLoading, setLoading] = useState(false);
    const [bucketName, setBucketName] = useState('');
    const [bucketNameValidationMessage, setBucketNameValidationMessage] = useState("");

    const handleSubmitInt = async (e : FormEvent<HTMLFormElement>) => {
        try
        {
            const addRes = await postApiV1B64Be5124B034028A58913931942E205AddBucket({requestBody: {ses: state.session, bucketName: bucketName}});
            state.pageAfterLogin = Pages.Buckets;
            
            await queryClient.invalidateQueries({ queryKey: ["buckets"] });
            
            await router.navigate(`/${state.pageAfterLogin}`);
        }
        catch(apiE)
        {
            if(apiE instanceof ApiError)
            {
            const e = apiE.body as HapiError;
            switch(e.herror)
            {
                case Herror.BucketAlreadyExists:
                setBucketNameValidationMessage("Bucket already exists");
                break;
            }
            }

            throw apiE;
        }
    }

    const handleSubmit = async (e : FormEvent<HTMLFormElement>) => {
        e.preventDefault();
        setLoading(true);
        try
        {
          await handleSubmitInt(e);
        }
        finally
        {
          setLoading(false);
        }
      }

    return <TableWrapper>
        <h3>Add Bucket</h3>
        <form onSubmit={handleSubmit}>
        <Field
            label="BucketName" required validationMessage={bucketNameValidationMessage}>
            <Input  id='BucketName' value={bucketName}  onChange={(e) => { setBucketName(e.target.value)}}/>
        </Field>
        {isLoading && <Spinner label="Creating bucket..." />}
        {!isLoading && <Button type="submit" icon={<AddRegular />} appearance="primary">Add Bucket</Button>}
        </form>
    </TableWrapper>
}

export default AddBucket;