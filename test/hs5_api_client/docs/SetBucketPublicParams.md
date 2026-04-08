# SetBucketPublicParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**bucket_name** | **str** |  | 
**public** | **List[str]** |  | 

## Example

```python
from hs5_api.models.set_bucket_public_params import SetBucketPublicParams

# TODO update the JSON string below
json = "{}"
# create an instance of SetBucketPublicParams from a JSON string
set_bucket_public_params_instance = SetBucketPublicParams.from_json(json)
# print the JSON string representation of the object
print(SetBucketPublicParams.to_json())

# convert the object into a dict
set_bucket_public_params_dict = set_bucket_public_params_instance.to_dict()
# create an instance of SetBucketPublicParams from a dict
set_bucket_public_params_from_dict = SetBucketPublicParams.from_dict(set_bucket_public_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


