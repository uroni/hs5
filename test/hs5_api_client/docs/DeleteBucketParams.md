# DeleteBucketParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**bucket_name** | **str** |  | 

## Example

```python
from hs5_api.models.delete_bucket_params import DeleteBucketParams

# TODO update the JSON string below
json = "{}"
# create an instance of DeleteBucketParams from a JSON string
delete_bucket_params_instance = DeleteBucketParams.from_json(json)
# print the JSON string representation of the object
print(DeleteBucketParams.to_json())

# convert the object into a dict
delete_bucket_params_dict = delete_bucket_params_instance.to_dict()
# create an instance of DeleteBucketParams from a dict
delete_bucket_params_from_dict = DeleteBucketParams.from_dict(delete_bucket_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


